// section: software. package count and pending updates
#include <jb.h>
int main(void) {
    const char *os = jb_os();
    jb_section("software");
    if (!strcmp(os, "linux")) {
        char *c = jb_run("dpkg-query -f '.\\n' -W 2>/dev/null | wc -l | grep -v '^0$' || rpm -qa 2>/dev/null | wc -l"); if (c && jb_number_in(c) > 0) printf("%-16s %lld\n", "packages", jb_number_in(c));
        char *up = jb_run("apt list --upgradable 2>/dev/null | grep -c upgradable"); if (up) printf("%-16s %lld\n", "updates", jb_number_in(up) < 0 ? 0 : jb_number_in(up));
    } else if (!strcmp(os, "macos")) {
        char *c = jb_run("ls /Applications 2>/dev/null | wc -l"); if (c) printf("%-16s %lld apps\n", "packages", jb_number_in(c));
        char *b = jb_run("brew list 2>/dev/null | wc -l"); if (b && jb_number_in(b) > 0) printf("%-16s %lld\n", "brew", jb_number_in(b));
    } else if (!strcmp(os, "windows")) {
        jb_row("packages", jb_first_line(jb_run("powershell -NoProfile -Command \"(Get-Package).Count\"")));
    }
    return 0;
}
