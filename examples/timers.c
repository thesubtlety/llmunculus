// q: what systemd timers are scheduled
#include <jb.h>
int main(void) {
    char *out = jb_run("systemctl list-timers --all --no-legend 2>/dev/null");   // next, left, last, passed, unit, activates
    if (!out) return 1;
    int n; char **v = jb_lines(out, &n);
    for (int i = 0; i < n; i++) if (v[i][0]) printf("%s\n", v[i]);
    printf("%d timers\n", n);
    return 0;
}
