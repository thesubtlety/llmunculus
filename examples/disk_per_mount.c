// q: how much disk space is used and free on each mount
#include <jb.h>
#include <sys/statvfs.h>
int main(void) {
    char *t = jb_read_file("/proc/mounts"); if (!t) return 1;
    int n; char **v = jb_lines(t, &n);
    for (int i = 0; i < n; i++) {
        char dev[256], mnt[256]; if (sscanf(v[i], "%255s %255s", dev, mnt) != 2 || dev[0] != '/') continue;   // real devices only
        struct statvfs s; if (statvfs(mnt, &s)) continue;
        unsigned long long total = (unsigned long long)s.f_blocks * s.f_frsize, avail = (unsigned long long)s.f_bavail * s.f_frsize;
        unsigned long long used = total - avail; printf("%s total=%llu free=%llu used=%d%%\n", mnt, total, avail, (used+avail) ? (int)(used * 100 / (used + avail)) : 0);   // df-style: over used+avail, not total
    }
    return 0;
}
