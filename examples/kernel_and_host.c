// q: what is the kernel version and hostname
#include <jb.h>
#include <sys/utsname.h>
int main(void) {
    struct utsname u; if (uname(&u)) return 1;
    printf("%s %s %s %s\n", u.sysname, u.release, u.machine, u.nodename);
    return 0;
}
