// q: what is the maximum process id on this system
#include <jb.h>
int main(void) {
    char *t = jb_read_file("/proc/sys/kernel/pid_max"); if (!t) return 1;   // kernel settings are files under /proc/sys
    printf("%lld\n", jb_number_in(t));
    return 0;
}
