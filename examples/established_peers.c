// q: which remote addresses have established connections to this machine
#include <jb.h>
#include <arpa/inet.h>
int main(void) {
    char *t = jb_read_file("/proc/net/tcp"); if (!t) return 1;
    int n; char **v = jb_lines(t, &n);
    for (int i = 1; i < n; i++) {
        char local[64], rem[64]; unsigned st;
        if (sscanf(v[i], "%*d: %63s %63s %x", local, rem, &st) != 3 || st != 1) continue;   // 01 = ESTABLISHED
        unsigned ip, port, lip, lport; sscanf(rem, "%x:%x", &ip, &port); sscanf(local, "%x:%x", &lip, &lport);   // little-endian hex
        struct in_addr a = { .s_addr = ip }, l = { .s_addr = lip };
        printf("%s:%u <- local ", inet_ntoa(a), port); printf("%s:%u\n", inet_ntoa(l), lport);   // inet_ntoa reuses one buffer
    }
    return 0;
}
