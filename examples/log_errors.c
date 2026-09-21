// q: how many lines in /var/log/syslog mention error
#include <jb.h>
int main(void) {
    char *t = jb_read_file("/var/log/syslog");
    if (!t) t = jb_run("journalctl --no-pager -q 2>/dev/null");   // no syslog file: ask the journal
    if (!t) return 1;
    int n, c = 0; char **v = jb_lines(t, &n);
    for (int i = 0; i < n; i++) { for (char *p = v[i]; *p; p++) *p = tolower((unsigned char)*p); if (strstr(v[i], "error")) c++; }
    printf("%d\n", c);
    return 0;
}
