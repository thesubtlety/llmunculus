// q: which version of a package is installed
#include <jb.h>
int main(void) {
    const char *pkg = "openssh-server";
    char *out = jb_run("dpkg-query -W -f '${Version}' openssh-server 2>/dev/null");   // debian, ubuntu
    if (!out || !*out) out = jb_run("rpm -q --qf '%{VERSION}' openssh-server 2>/dev/null");
    if (!out || !*out) { printf("%s not installed\n", pkg); return 0; }
    printf("%s %s\n", pkg, jb_trim(out));
    return 0;
}
