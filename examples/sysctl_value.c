// q: what is a kernel parameter set to, like ip_forward
#include <jb.h>
int main(void) {
    char *t = jb_read_file("/proc/sys/net/ipv4/ip_forward");   // sysctl net.ipv4.ip_forward is this file
    if (!t) return 1;
    printf("net.ipv4.ip_forward = %s\n", jb_trim(t));
    return 0;
}
