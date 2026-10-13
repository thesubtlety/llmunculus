// q: is the root filesystem / over 90 percent full
#include <jb.h>
#include <sys/statvfs.h>
int main(void) {
    struct statvfs v; if (statvfs("/", &v)) { perror("statvfs"); return 1; }
    unsigned long long avail = (unsigned long long)v.f_bavail * v.f_frsize;
    unsigned long long used  = (unsigned long long)(v.f_blocks - v.f_bfree) * v.f_frsize;
    unsigned long long cap = used + avail;   // df-style: exclude root-reserved blocks
    printf("/ is %d%% full\n", cap ? (int)(used * 100 / cap) : 0);
    return 0;
}
