// q: show the last lines of a log file
#include <jb.h>
int main(void) {
    char *t = jb_read_file("/var/log/syslog"); if (!t) t = jb_run("journalctl --no-pager -q -n 10 2>/dev/null");
    if (!t) return 1;
    int n; char **v = jb_lines(t, &n);
    for (int i = n > 10 ? n - 10 : 0; i < n; i++) printf("%s\n", v[i]);
    return 0;
}
