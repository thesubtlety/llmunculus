// q: which services are enabled at boot but not running
#include <jb.h>
int main(void) {
    char *out = jb_run("systemctl list-unit-files --type=service --state=enabled --no-legend 2>/dev/null");
    if (!out) return 1;
    int n, c = 0; char **v = jb_lines(out, &n);
    for (int i = 0; i < n; i++) {
        char unit[256]; if (sscanf(v[i], "%255s", unit) != 1) continue;
        char cmd[512]; snprintf(cmd, sizeof cmd, "systemctl is-active %s 2>/dev/null", unit);
        char *st = jb_run(cmd);
        if (st && strcmp(jb_trim(st), "active")) { printf("%s %s\n", unit, jb_trim(st)); c++; }
    }
    printf("%d enabled but not active\n", c);
    return 0;
}
