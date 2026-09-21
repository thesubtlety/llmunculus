// q: what are the ip addresses of this machine
#include <jb.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <netinet/in.h>
int main(void) {
    struct ifaddrs *list; if (getifaddrs(&list)) return 1;
    for (struct ifaddrs *a = list; a; a = a->ifa_next) {
        if (!a->ifa_addr || a->ifa_addr->sa_family != AF_INET) continue;
        char ip[64]; inet_ntop(AF_INET, &((struct sockaddr_in *)a->ifa_addr)->sin_addr, ip, sizeof ip);
        printf("%s %s\n", a->ifa_name, ip);
    }
    freeifaddrs(list);
    return 0;
}
