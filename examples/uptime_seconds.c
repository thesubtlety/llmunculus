// q: how long has this machine been up
#include <jb.h>
int main(void) {
    char *t = jb_read_file("/proc/uptime"); if (!t) return 1;   // "584177.32 2201234.10"
    long long s = jb_number_in(t);
    printf("%lld seconds (%lld days %lld hours)\n", s, s / 86400, s % 86400 / 3600);
    return 0;
}
