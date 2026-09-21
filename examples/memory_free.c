// q: how much memory and swap is there, free and available
#include <jb.h>
int main(void) {
    char *t = jb_read_file("/proc/meminfo"); if (!t) return 1;
    long long total = jb_number_in(jb_kv(t, "MemTotal", ':')), avail = jb_number_in(jb_kv(t, "MemAvailable", ':')), swap = jb_number_in(jb_kv(t, "SwapTotal", ':'));
    printf("total=%lld available=%lld swap=%lld bytes\n", total * 1024, avail * 1024, swap * 1024);   // meminfo is in kB
    return 0;
}
