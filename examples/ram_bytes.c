// q: how much RAM does this machine have in bytes
#include <jb.h>
int main(void) {
    char *t = jb_read_file("/proc/meminfo"); if (!t) return 1;
    long long kb = jb_number_in(jb_kv(t, "MemTotal", ':'));   // "MemTotal:  7937232 kB"
    printf("%lld\n", kb * 1024);
    return 0;
}
