// q: resolve a hostname to an ip via dns
#include <jb.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
int main(void) {
    struct addrinfo hints = { 0 }, *r; hints.ai_family = AF_INET;
    if (getaddrinfo("example.com", NULL, &hints, &r)) { printf("cannot resolve\n"); return 1; }
    char ip[64]; inet_ntop(AF_INET, &((struct sockaddr_in *)r->ai_addr)->sin_addr, ip, sizeof ip);
    printf("%s\n", ip); freeaddrinfo(r);
    return 0;
}
