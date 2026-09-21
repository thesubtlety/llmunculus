/* jb.h: helpers for small programs. all static, no linking. every function that returns a pointer returns
   NULL on failure, every int function returns -1 on failure. strings are malloc'd, freeing is optional. */
#ifndef JB_H
#define JB_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>

/* which OS this is running on: "linux", "macos", "windows", or "unix". from uname, works in one portable binary */
#include <sys/utsname.h>
static const char *jb_os(void) {
    struct utsname u; if (uname(&u)) return "unix";
    if (strstr(u.sysname, "Linux")) return "linux";
    if (strstr(u.sysname, "Darwin")) return "macos";
    if (strstr(u.sysname, "Windows") || strstr(u.sysname, "NT")) return "windows";
    return "unix";
}
/* home directory of the current user, without a terminal (getpwuid, not getlogin) */
static const char *jb_home(void) {
    const char *h = getenv("HOME"); if (h && *h) return h;
    struct passwd *p = getpwuid(getuid()); return p ? p->pw_dir : "/";
}
/* expand a leading ~ to the home directory. returns a static buffer, so copy it if you keep it */
static const char *jb_expand(const char *path) {
    static char buf[4096];
    if (path[0] == '~' && (path[1] == '/' || path[1] == 0)) { snprintf(buf, sizeof buf, "%s%s", jb_home(), path + 1); return buf; }
    return path;
}
/* whole file as a string. a leading ~ is expanded */
static char *jb_read_file(const char *path) {
    FILE *f = fopen(jb_expand(path), "rb"); if (!f) return NULL;
    size_t cap = 65536, n = 0; char *s = malloc(cap);
    for (size_t k; (k = fread(s + n, 1, cap - n - 1, f)) > 0;) { n += k; if (n + 1 >= cap) s = realloc(s, cap *= 2); }
    fclose(f); s[n] = 0; return s;
}
/* write or append text to a file. a leading ~ is expanded. returns bytes written */
static int jb_write_file(const char *path, const char *text, int append) {
    FILE *f = fopen(jb_expand(path), append ? "a" : "w"); if (!f) return -1;
    int n = fputs(text, f) < 0 ? -1 : (int)strlen(text); fclose(f); return n;
}
/* run a command, return its output. popen's built-in shell knows pipes, redirects, && and ||, but not $(...), subshells,
   for or if. where a real shell exists it gets the command instead, so the whole language works. */
static char *jb_run(const char *cmd) {
    char *full = NULL;
    if (access("/bin/sh", X_OK) == 0) {
        size_t n = strlen(cmd), k = 0; full = malloc(4 * n + 16);   // each quote escapes to 4 bytes
        memcpy(full, "/bin/sh -c '", 12); k = 12;
        for (const char *c = cmd; *c; c++) { if (*c == '\'') { memcpy(full + k, "'\\''", 4); k += 4; } else full[k++] = *c; }
        full[k++] = '\''; full[k] = 0;
    }
    FILE *p = popen(full ? full : cmd, "r"); free(full);
    if (!p) return NULL;
    size_t cap = 65536, n = 0; char *s = malloc(cap);
    for (size_t k; (k = fread(s + n, 1, cap - n - 1, p)) > 0;) { n += k; if (n + 1 >= cap) s = realloc(s, cap *= 2); }
    pclose(p); s[n] = 0; return s;
}
/* split text into lines. *n gets the count. lines[i] are NUL terminated */
static char **jb_lines(const char *text, int *n) {
    char *copy = strdup(text ? text : ""); int cap = 64, k = 0; char **v = malloc(cap * sizeof *v);
    for (char *p = copy; *p;) { char *e = strchr(p, '\n'); if (e) *e = 0; if (k + 1 >= cap) v = realloc(v, (cap *= 2) * sizeof *v); v[k++] = p; if (!e) break; p = e + 1; }
    v[k] = NULL; *n = k; return v;
}
/* first line of text that starts with prefix, or NULL */
static char *jb_line_starting(const char *text, const char *prefix) {
    int n; char **v = jb_lines(text, &n);
    for (int i = 0; i < n; i++) if (!strncmp(v[i], prefix, strlen(prefix))) return v[i];
    return NULL;
}
/* how many lines contain needle */
static int jb_count_lines_with(const char *text, const char *needle) {
    int n, c = 0; char **v = jb_lines(text, &n);
    for (int i = 0; i < n; i++) if (strstr(v[i], needle)) c++;
    return c;
}
/* value after "key<sep>" on its line, trimmed. e.g. jb_kv(meminfo, "MemTotal", ':') -> "8127725568 kB" */
static char *jb_kv(const char *text, const char *key, char sep) {
    char *l = jb_line_starting(text, key); if (!l) return NULL;
    l += strlen(key); while (*l == ' ' || *l == '\t') l++;   // "model name\t: x" and "MemTotal: x" both
    if (*l != sep) return NULL;
    l++; while (*l == ' ' || *l == '\t') l++;
    if (*l == '"') { l++; char *q = strchr(l, '"'); if (q) *q = 0; }
    return l;
}
/* first integer in a string, or -1 */
static long long jb_number_in(const char *s) {
    if (!s) return -1;
    while (*s && !isdigit((unsigned char)*s) && !(*s == '-' && isdigit((unsigned char)s[1]))) s++;
    return *s ? strtoll(s, NULL, 10) : -1;
}
/* trim spaces and newlines at both ends, in place */
static char *jb_trim(char *s) {
    if (!s) return s;
    while (isspace((unsigned char)*s)) s++;
    char *e = s + strlen(s); while (e > s && isspace((unsigned char)e[-1])) *--e = 0;
    return s;
}
/* value of KEY in a KEY=VALUE file (shell, env, or .conf). handles a leading "export " and quotes. ~ is expanded. NULL if absent */
static char *jb_conf_get(const char *path, const char *key) {
    char *t = jb_read_file(path); if (!t) return NULL;
    int n; char **v = jb_lines(t, &n);
    for (int i = 0; i < n; i++) {
        char *l = jb_trim(v[i]);
        if (!strncmp(l, "export ", 7)) l += 7;
        if (strncmp(l, key, strlen(key))) continue;
        char *p = l + strlen(key); while (*p == ' ' || *p == '	') p++;
        if (*p != '=' && *p != ':') continue;
        p++; while (*p == ' ' || *p == '	') p++;
        if (*p == '"' || *p == '\'') { char q = *p++; char *e = strchr(p, q); if (e) *e = 0; }
        return p;
    }
    return NULL;
}
/* entries in a directory, skipping . and .. . *n gets the count. NULL if it cannot be opened */
static char **jb_dir_list(const char *path, int *n) {
    DIR *d = opendir(path); if (!d) return NULL;
    int cap = 64, k = 0; char **v = malloc(cap * sizeof *v); struct dirent *e;
    while ((e = readdir(d))) { if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue; if (k + 1 >= cap) v = realloc(v, (cap *= 2) * sizeof *v); v[k++] = strdup(e->d_name); }
    closedir(d); v[k] = NULL; *n = k; return v;
}
/* 1 if path exists, 2 if it is a directory, 0 if not */
static int jb_exists(const char *path) { struct stat st; return stat(path, &st) ? 0 : S_ISDIR(st.st_mode) ? 2 : 1; }
/* file size in bytes, or -1 */
static long long jb_file_size(const char *path) { struct stat st; return stat(path, &st) ? -1 : (long long)st.st_size; }
/* connect a TCP socket to host:port with a timeout in ms. returns fd, or -1. no hang on a dead host. */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>
static int jb_tcp_connect(const char *host, int port, int timeout_ms) {
    char p[16]; snprintf(p, sizeof p, "%d", port);
    struct addrinfo hints, *ai = 0; memset(&hints, 0, sizeof hints); hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, p, &hints, &ai) || !ai) return -1;
    int fd = socket(ai->ai_family, SOCK_STREAM, 0);
    if (fd < 0) { freeaddrinfo(ai); return -1; }
    int fl = fcntl(fd, F_GETFL, 0); fcntl(fd, F_SETFL, fl | O_NONBLOCK);
    int rc = connect(fd, ai->ai_addr, ai->ai_addrlen);
    freeaddrinfo(ai);
    if (rc < 0 && errno == EINPROGRESS) {
        struct pollfd pf = { fd, POLLOUT, 0 };
        if (poll(&pf, 1, timeout_ms) != 1) { close(fd); return -1; }        // timed out or error
        int err = 0; socklen_t el = sizeof err; getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &el);
        if (err) { close(fd); return -1; }
    } else if (rc < 0) { close(fd); return -1; }
    fcntl(fd, F_SETFL, fl);   // back to blocking for the caller
    return fd;
}
/* one-shot HTTPS. method "GET"/"POST", url https://..., body/ctype NULL for GET. response body -> out. returns HTTP status or -1.
   verifies the certificate against the system CA bundle if present; JB_TLS_INSECURE=1 skips verification. native, no curl. */
int jb_https(const char *method, const char *url, const char *body, const char *ctype, char *out, unsigned long cap);
/* profile output: a section header and aligned key/value rows */
static void jb_section(const char *name) { printf("\n== %s ==\n", name); }
static void jb_row(const char *k, const char *v) { printf("%-16s %s\n", k, v && *v ? v : "?"); }
/* first line of a string, trimmed and in place. handy for one-line command output */
static char *jb_first_line(char *s) { if (s) { s[strcspn(s, "\n")] = 0; return jb_trim(s); } return s; }
#endif
