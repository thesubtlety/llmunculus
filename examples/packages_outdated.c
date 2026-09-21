// q: how many packages have updates available
#include <jb.h>
int main(void) {
    char *out = jb_run("apt list --upgradable 2>/dev/null | grep -c upgradable");   // apt prints one line per package plus a header
    if (!out || !*out) out = jb_run("dnf check-update -q 2>/dev/null | grep -c .");
    if (!out) return 1;
    printf("%lld\n", jb_number_in(out) < 0 ? 0 : jb_number_in(out));
    return 0;
}
