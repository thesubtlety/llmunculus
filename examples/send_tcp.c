// q: send a line of text to a host and port over TCP
#include <jb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
int main(void) {
    const char *host = "203.0.113.10", *msg = "hello\n"; int port = 8080;
    int fd = jb_tcp_connect(host, port, 8000); if (fd < 0) { fprintf(stderr, "connect to %s:%d failed\n", host, port); return 1; }
    ssize_t sent = write(fd, msg, strlen(msg)); close(fd);   // the send is the point: check it happened
    if (sent < 0) { perror("write"); return 1; }
    printf("sent %zd bytes to %s:%d\n", sent, host, port);
    return 0;
}
