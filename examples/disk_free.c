// q: how many bytes of disk space are free on the root filesystem /
#include <jb.h>
#include <sys/statvfs.h>
int main(void) {
    struct statvfs v; if (statvfs("/", &v)) { perror("statvfs"); return 1; }
    printf("%llu\n", (unsigned long long)v.f_bavail * v.f_frsize);   // f_bavail: blocks a normal user can use
    return 0;
}
