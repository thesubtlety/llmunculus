// delivery is our code, not the model's. written once, the same on every run.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <syslog.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include "report.h"
int jb_https(const char *, const char *, const char *, const char *, char *, unsigned long);

// connect with a timeout, so a report to a dead host does not hang the agent
#include <fcntl.h>
#include <poll.h>
static int connect_timeout(const char *host, const char *port, int ms) {
    struct addrinfo hints, *ai = 0; memset(&hints, 0, sizeof hints); hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, port, &hints, &ai) || !ai) return -1;
    int fd = socket(ai->ai_family, SOCK_STREAM, 0);
    if (fd < 0) { freeaddrinfo(ai); return -1; }
    int fl = fcntl(fd, F_GETFL, 0); fcntl(fd, F_SETFL, fl | O_NONBLOCK);
    int rc = connect(fd, ai->ai_addr, ai->ai_addrlen);
    freeaddrinfo(ai);
    if (rc < 0 && errno == EINPROGRESS) {
        struct pollfd pf = { fd, POLLOUT, 0 };
        if (poll(&pf, 1, ms) != 1) { close(fd); return -1; }
        int e = 0; socklen_t el = sizeof e; getsockopt(fd, SOL_SOCKET, SO_ERROR, &e, &el);
        if (e) { close(fd); return -1; }
    } else if (rc < 0) { close(fd); return -1; }
    fcntl(fd, F_SETFL, fl);
    return fd;
}

static size_t jstr(char *o, size_t cap, const char *s) {   // JSON string with escapes. returns bytes written
    size_t n = 0;
    if (n < cap) { o[n++] = '"'; }
    for (; *s && n + 7 < cap; s++) {
        unsigned char c = *s;
        if (c == '"' || c == '\\') { o[n++] = '\\'; o[n++] = c; }
        else if (c == '\n') { o[n++] = '\\'; o[n++] = 'n'; }
        else if (c == '\r') { o[n++] = '\\'; o[n++] = 'r'; }
        else if (c == '\t') { o[n++] = '\\'; o[n++] = 't'; }
        else if (c < 0x20) { n += snprintf(o + n, cap - n, "\\u%04x", c); }
        else { o[n++] = c; }
    }
    if (n < cap) { o[n++] = '"'; }
    if (n < cap) { o[n] = 0; }
    return n;
}

// bounded append: never let n run past cap so the next jstr(out+n, cap-n) can't underflow
#define APP(...) do { if (n < cap) n += snprintf(out + n, cap - n, __VA_ARGS__); if (n >= cap) { out[cap-1]=0; return -1; } } while (0)
#define APPS(s) do { if (n < cap) n += jstr(out + n, cap - n, s); if (n >= cap) { out[cap-1]=0; return -1; } } while (0)
int report_json(char *out, size_t cap, const char *host, const char *task, const char *answer, const char *facts, int failed, const char *verdict) {
    char ts[32]; time_t t = time(NULL); strftime(ts, sizeof ts, "%Y-%m-%dT%H:%M:%SZ", gmtime(&t));
    size_t n = snprintf(out, cap, "{\"time\":\"%s\",\"host\":", ts); if (n >= cap) return -1;
    APPS(host);
    APP(",\"task\":"); APPS(task);
    char a[2048]; snprintf(a, sizeof a, "%s", answer); a[strcspn(a, "\n")] = 0;   // the sentence only, facts go separately
    APP(",\"answer\":"); APPS(a);
    APP(",\"facts\":[");
    int first = 1;
    for (const char *p = facts; p && *p; ) {
        const char *nl = strchr(p, '\n'); size_t len = nl ? (size_t)(nl - p) : strlen(p);
        if (len > 2 && p[0] == '-' && p[1] == ' ') {
            char f[1024]; size_t k = len - 2 < sizeof f - 1 ? len - 2 : sizeof f - 1; memcpy(f, p + 2, k); f[k] = 0;
            if (!first) { APP(","); } first = 0;
            APPS(f);
        }
        if (!nl) break;
        p = nl + 1;
    }
    APP("],\"failed\":%s,\"verdict\":", failed ? "true" : "false");
    if (verdict) APPS(verdict); else APP("null");
    APP("}");
    return (int)n;
}

// plain http POST with our own socket. used when curl is missing. no TLS here: https needs curl.
static int http_post(const char *url, const char *body, char *err, size_t errcap) {
    char host[256], path[1024] = "/", port[8] = "80";
    const char *p = url + 7, *slash = strchr(p, '/');
    size_t hl = slash ? (size_t)(slash - p) : strlen(p);
    if (hl >= sizeof host) { snprintf(err, errcap, "url too long"); return -1; }
    memcpy(host, p, hl); host[hl] = 0;
    if (slash) snprintf(path, sizeof path, "%s", slash);
    char *colon = strchr(host, ':'); if (colon) { *colon = 0; snprintf(port, sizeof port, "%s", colon + 1); }
    int fd = connect_timeout(host, port, 15000);   // 15s connect timeout, no hang on a dead host
    if (fd < 0) { snprintf(err, errcap, "connect %s:%s failed", host, port); return -1; }
    char req[2048]; int rl = snprintf(req, sizeof req, "POST %s HTTP/1.0\r\nHost: %s\r\nContent-Type: application/json\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n", path, host, strlen(body));
    if (rl < 0 || rl >= (int)sizeof req) { snprintf(err, errcap, "request too long"); close(fd); return -1; }   // guard the header buffer
    if (write(fd, req, rl) != rl || write(fd, body, strlen(body)) != (ssize_t)strlen(body)) { snprintf(err, errcap, "write: %s", strerror(errno)); close(fd); return -1; }
    char resp[256] = ""; ssize_t k = read(fd, resp, sizeof resp - 1); close(fd);
    if (k <= 0) { snprintf(err, errcap, "no response"); return -1; }
    resp[k] = 0;
    int code = 0; sscanf(resp, "HTTP/%*s %d", &code);
    if (code < 200 || code > 299) { snprintf(err, errcap, "http %d", code); return -1; }
    return 0;
}

int report_send(const char *dest, const char *json, char *err, size_t errcap) {
    err[0] = 0;
    if (!strcmp(dest, "stdout")) { puts(json); return 0; }
    if (!strncmp(dest, "file:", 5)) {
        FILE *f = fopen(dest + 5, "a");
        if (!f) { snprintf(err, errcap, "%s: %s", dest + 5, strerror(errno)); return -1; }
        fprintf(f, "%s\n", json); fclose(f); return 0;
    }
    if (!strcmp(dest, "syslog")) { openlog("justabuilder", LOG_PID, LOG_USER); syslog(LOG_INFO, "%s", json); closelog(); return 0; }
    if (!strncmp(dest, "https://", 8)) {
        char resp[4096]; int code = jb_https("POST", dest, json, "application/json", resp, sizeof resp);   // native TLS, no curl
        if (code / 100 == 2) return 0;
        snprintf(err, errcap, code < 0 ? "tls request failed" : "endpoint returned http %d", code);
        return -1;
    }
    if (!strncmp(dest, "http://", 7)) return http_post(dest, json, err, errcap);   // native plaintext
    snprintf(err, errcap, "unknown report destination: %s (stdout | file:PATH | syslog | http(s)://...)", dest);
    return -1;
}
