// q: how many filesystems are mounted
#include <jb.h>
int main(void) {
    char *t = jb_read_file("/proc/mounts"); if (!t) return 1;
    int n; jb_lines(t, &n);   // one mount per line
    printf("%d\n", n);
    return 0;
}
