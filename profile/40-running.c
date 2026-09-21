// section: running. process count, failed services, top by memory, containers
#include <jb.h>
int main(void) {
    const char *os = jb_os();
    jb_section("running");
    if (!strcmp(os, "linux")) {
        int n, p = 0; char **e = jb_dir_list("/proc", &n); if (e) { for (int i = 0; i < n; i++) if (e[i][0] >= '0' && e[i][0] <= '9') p++; printf("%-16s %d\n", "processes", p); }
        char *f = jb_run("systemctl list-units --state=failed --no-legend 2>/dev/null | wc -l"); if (f) jb_row("failed units", jb_trim(f));
        char *t = jb_run("ps -eo comm,rss --sort=-rss 2>/dev/null | awk 'NR>1 && NR<=6{printf \"%s(%dMB) \",$1,$2/1024}'"); if (t && *jb_trim(t)) jb_row("top memory", t);
        char *d = jb_run("docker ps --format '{{.Names}}' 2>/dev/null | tr '\\n' ' '"); if (d && *jb_trim(d)) jb_row("containers", d);
    } else if (!strcmp(os, "macos")) {
        char *c = jb_run("ps -A | wc -l"); if (c) printf("%-16s %lld\n", "processes", jb_number_in(c) - 1);
        char *t = jb_run("ps -Ao comm,rss -m 2>/dev/null | awk 'NR>1 && NR<=6{printf \"%s(%dMB) \",$1,$2/1024}'"); if (t && *jb_trim(t)) jb_row("top memory", t);
    } else if (!strcmp(os, "windows")) {
        char *c = jb_run("powershell -NoProfile -Command \"(Get-Process).Count\""); if (c) jb_row("processes", jb_first_line(c));
        char *t = jb_run("powershell -NoProfile -Command \"Get-Process | Sort-Object WS -Descending | Select-Object -First 5 | ForEach-Object { '{0}({1}MB)' -f $_.Name,[int]($_.WS/1MB) }\" | tr '\\n' ' '"); if (t && *jb_trim(t)) jb_row("top memory", t);
    }
    return 0;
}
