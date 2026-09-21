// q: on macOS, which launchd services are running
#include <jb.h>
int main(void) {
    char *out = jb_run("launchctl list 2>/dev/null");   // pid status label. pid is - when not running
    if (!out) return 1;
    int n, running = 0; char **v = jb_lines(out, &n);
    for (int i = 1; i < n; i++) if (v[i][0] != '-') running++;
    printf("%d running of %d\n", running, n - 1);
    return 0;
}
