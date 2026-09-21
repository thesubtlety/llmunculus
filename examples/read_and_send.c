// q: read a variable like ITSHARE_ENDPOINT from a file like ~/.itsvcs and send it to a host and port
#include <jb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
int main(void) {
    const char *key = "ITSHARE_ENDPOINT", *host = "203.0.113.10"; int port = 8080;
    char *val = jb_conf_get("~/.itsvcs", key); if (!val) { fprintf(stderr, "%s not found\n", key); return 1; }
    int fd = jb_tcp_connect(host, port, 8000); if (fd < 0) { fprintf(stderr, "connect to %s:%d failed\n", host, port); return 1; }
    char line[512]; int len = snprintf(line, sizeof line, "%s=%s\n", key, val);
    ssize_t sent = write(fd, line, len); close(fd);   // the send is the point
    if (sent < 0) { perror("write"); return 1; }
    printf("sent %s=%s to %s:%d\n", key, val, host, port);
    return 0;
}
