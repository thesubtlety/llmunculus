// q: how many packages are installed
#include <jb.h>
int main(void) {
    char *out = jb_run("dpkg-query -f '${binary:Package}\n' -W 2>/dev/null");   // debian and ubuntu
    if (!out || !*out) out = jb_run("rpm -qa 2>/dev/null");                       // fedora, rhel
    if (!out || !*out) out = jb_run("pacman -Q 2>/dev/null");                     // arch
    if (!out) return 1;
    int n; jb_lines(out, &n);
    printf("%d\n", n);
    return 0;
}
