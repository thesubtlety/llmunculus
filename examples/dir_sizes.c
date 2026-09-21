// q: which subdirectories use the most disk space
#include <jb.h>
int main(void) {
    char *out = jb_run("du -sk /var/log/* 2>/dev/null | sort -rn | head -5");   // kB, largest first
    if (!out) return 1;
    int n; char **v = jb_lines(out, &n);
    for (int i = 0; i < n; i++) { long long kb; char path[1024]; if (sscanf(v[i], "%lld %1023s", &kb, path) == 2) printf("%lld bytes %s\n", kb * 1024, path); }
    return 0;
}
