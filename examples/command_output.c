// q: how many systemd units are in a failed state
#include <jb.h>
int main(void) {
    char *out = jb_run("systemctl list-units --state=failed --no-legend 2>/dev/null");   // one unit per line, no header
    if (!out) return 1;
    int n; jb_lines(out, &n);
    printf("%d\n", n);
    return 0;
}
