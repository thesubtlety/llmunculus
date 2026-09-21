// native HTTPS with mbedtls, so reports and probes reach https endpoints without shelling out to curl.
// one-shot request: connect, TLS handshake, send, read to EOF (HTTP/1.0, Connection: close), return the status code.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/x509_crt.h"
#include <sys/socket.h>
#include <netdb.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <errno.h>
static int tls_connect(const char *host, const char *port, int ms) {
    struct addrinfo hints, *ai = 0; memset(&hints, 0, sizeof hints); hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, port, &hints, &ai) || !ai) return -1;
    int fd = socket(ai->ai_family, SOCK_STREAM, 0);
    if (fd < 0) { freeaddrinfo(ai); return -1; }
    int fl = fcntl(fd, F_GETFL, 0); fcntl(fd, F_SETFL, fl | O_NONBLOCK);
    int rc = connect(fd, ai->ai_addr, ai->ai_addrlen); freeaddrinfo(ai);
    if (rc < 0 && errno == EINPROGRESS) { struct pollfd pf = { fd, POLLOUT, 0 };
        if (poll(&pf, 1, ms) != 1) { close(fd); return -1; }
        int e = 0; socklen_t el = sizeof e; getsockopt(fd, SOL_SOCKET, SO_ERROR, &e, &el); if (e) { close(fd); return -1; } }
    else if (rc < 0) { close(fd); return -1; }
    fcntl(fd, F_SETFL, fl); return fd;
}

// common CA bundle locations. if one loads, we verify the certificate; if none, we proceed unverified (like curl -k).
static const char *CA_PATHS[] = {
    "/etc/ssl/certs/ca-certificates.crt", "/etc/pki/tls/certs/ca-bundle.crt",
    "/etc/ssl/cert.pem", "/etc/ssl/certs/ca-bundle.crt", 0 };

// method e.g. "GET" or "POST"; url like https://host[:port]/path; body/ctype may be NULL for GET.
// the response body is written to out (capped). returns the HTTP status code, or -1 on a transport error.
int jb_https(const char *method, const char *url, const char *body, const char *ctype, char *out, unsigned long cap) {
    if (strncmp(url, "https://", 8)) return -1;
    char host[256], port[8] = "443", path[1024] = "/";
    const char *p = url + 8, *slash = strchr(p, '/');
    size_t hl = slash ? (size_t)(slash - p) : strlen(p);
    if (hl >= sizeof host) return -1;
    memcpy(host, p, hl); host[hl] = 0;
    if (slash) snprintf(path, sizeof path, "%s", slash);
    char *colon = strchr(host, ':'); if (colon) { *colon = 0; snprintf(port, sizeof port, "%s", colon + 1); }

    mbedtls_net_context net; mbedtls_ssl_context ssl; mbedtls_ssl_config conf;
    mbedtls_entropy_context ent; mbedtls_ctr_drbg_context drbg; mbedtls_x509_crt ca;
    mbedtls_net_init(&net); mbedtls_ssl_init(&ssl); mbedtls_ssl_config_init(&conf);
    mbedtls_entropy_init(&ent); mbedtls_ctr_drbg_init(&drbg); mbedtls_x509_crt_init(&ca);
    int rc = -1, status = -1;

    if (mbedtls_ctr_drbg_seed(&drbg, mbedtls_entropy_func, &ent, (const unsigned char *)"llmunculus", 12)) goto done;
    net.fd = tls_connect(host, port, 15000);   // 15s connect timeout, no hang on a dead host
    if (net.fd < 0) goto done;
    if (mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT)) goto done;

    int have_ca = 0;
    if (!getenv("JB_TLS_INSECURE")) for (int i = 0; CA_PATHS[i]; i++) if (mbedtls_x509_crt_parse_file(&ca, CA_PATHS[i]) == 0) { have_ca = 1; break; }
    mbedtls_ssl_conf_authmode(&conf, have_ca ? MBEDTLS_SSL_VERIFY_REQUIRED : MBEDTLS_SSL_VERIFY_NONE);
    if (have_ca) mbedtls_ssl_conf_ca_chain(&conf, &ca, NULL);
    mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &drbg);
    if (mbedtls_ssl_setup(&ssl, &conf)) goto done;
    mbedtls_ssl_set_hostname(&ssl, host);                                  // SNI and cert name check
    mbedtls_ssl_set_bio(&ssl, &net, mbedtls_net_send, mbedtls_net_recv, NULL);

    int hs; while ((hs = mbedtls_ssl_handshake(&ssl)) != 0) if (hs != MBEDTLS_ERR_SSL_WANT_READ && hs != MBEDTLS_ERR_SSL_WANT_WRITE) goto done;

    // header first, then the body as its own write. never concatenate body into a fixed buffer (would leak stack past it).
    char req[2048]; int rl;
    if (body) rl = snprintf(req, sizeof req, "%s %s HTTP/1.0\r\nHost: %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n",
                            method, path, host, ctype ? ctype : "application/json", strlen(body));
    else rl = snprintf(req, sizeof req, "%s %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n", method, path, host);
    if (rl < 0 || rl >= (int)sizeof req) goto done;
    for (int off = 0; off < rl; ) { int w = mbedtls_ssl_write(&ssl, (unsigned char *)req + off, rl - off); if (w <= 0) { if (w == MBEDTLS_ERR_SSL_WANT_WRITE) continue; goto done; } off += w; }
    if (body) for (size_t off = 0, bl = strlen(body); off < bl; ) { int w = mbedtls_ssl_write(&ssl, (const unsigned char *)body + off, bl - off); if (w <= 0) { if (w == MBEDTLS_ERR_SSL_WANT_WRITE) continue; goto done; } off += w; }

    // read the whole response, then split status line and body
    char *resp = malloc(1 << 16); size_t n = 0, rcap = 1 << 16; const size_t RMAX = 1 << 20;   // 1 MB ceiling: a hostile endpoint can't OOM the agent
    if (!resp) goto done;
    for (;;) { int r = mbedtls_ssl_read(&ssl, (unsigned char *)resp + n, rcap - n - 1); if (r == MBEDTLS_ERR_SSL_WANT_READ) continue; if (r <= 0) break; n += r;
               if (n + 1 >= rcap) { if (rcap >= RMAX) break; char *nr = realloc(resp, rcap *= 2); if (!nr) break; resp = nr; } }
    resp[n] = 0;
    sscanf(resp, "HTTP/%*s %d", &status);
    char *hdrend = strstr(resp, "\r\n\r\n");
    const char *bodystart = hdrend ? hdrend + 4 : resp;
    snprintf(out, cap, "%s", bodystart);
    free(resp);
    rc = 0;

done:
    mbedtls_ssl_close_notify(&ssl);
    mbedtls_net_free(&net); mbedtls_ssl_free(&ssl); mbedtls_ssl_config_free(&conf);
    mbedtls_x509_crt_free(&ca); mbedtls_ctr_drbg_free(&drbg); mbedtls_entropy_free(&ent);
    (void)rc;
    return status;
}
