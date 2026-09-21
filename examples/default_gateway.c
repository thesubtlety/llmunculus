// q: what is the default gateway
#include <jb.h>
#include <arpa/inet.h>
int main(void) {
    char *t = jb_read_file("/proc/net/route"); if (!t) return 1;   // Iface Destination Gateway ... in little-endian hex
    int n; char **v = jb_lines(t, &n);
    for (int i = 1; i < n; i++) {
        char iface[32]; unsigned dest, gw;
        if (sscanf(v[i], "%31s %x %x", iface, &dest, &gw) == 3 && dest == 0) { struct in_addr a = { .s_addr = gw }; printf("%s via %s\n", inet_ntoa(a), iface); return 0; }
    }
    printf("no default route\n");
    return 0;
}
