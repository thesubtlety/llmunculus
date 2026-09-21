// section: disk. free and total per mount
#include <jb.h>
#include <sys/statvfs.h>
static void one(const char *mnt) { struct statvfs s; if (statvfs(mnt, &s) || s.f_blocks == 0) return;
    unsigned long long tot = (unsigned long long)s.f_blocks*s.f_frsize, av = (unsigned long long)s.f_bavail*s.f_frsize;
    printf("%-16s %llu MB free of %llu MB (%d%% used)\n", mnt, av>>20, tot>>20, (int)(100 - av*100/tot)); }
int main(void) {
    const char *os = jb_os();
    jb_section("disk");
    if (!strcmp(os, "linux")) {
        char *m = jb_read_file("/proc/mounts"); if (!m) return 0; int n; char **v = jb_lines(m, &n);
        for (int i = 0; i < n; i++) { char dev[256], mnt[256]; if (sscanf(v[i], "%255s %255s", dev, mnt) == 2 && dev[0] == '/') one(mnt); }
    } else if (!strcmp(os, "macos")) {
        char *m = jb_run("mount | awk '{print $3}'"); if (!m) return 0; int n; char **v = jb_lines(m, &n);
        for (int i = 0; i < n; i++) if (v[i][0] == '/') one(v[i]);
    } else if (!strcmp(os, "windows")) {
        printf("%s", jb_run("powershell -NoProfile -Command \"Get-PSDrive -PSProvider FileSystem | ForEach-Object { '{0}: {1} MB free of {2} MB' -f $_.Name, [int]($_.Free/1MB), [int](($_.Used+$_.Free)/1MB) }\""));
    }
    return 0;
}
