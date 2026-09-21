// q: how much disk space is free on a remote host over ssh
#include <jb.h>
int main(void) {
    const char *host = "user@203.0.113.10";
    char cmd[256]; snprintf(cmd, sizeof cmd, "ssh -o BatchMode=yes -o ConnectTimeout=8 %s 'df -B1 / | tail -1' 2>&1", host);
    char *out = jb_run(cmd); if (!out || !*out) return 1;
    long long avail; char fs[128], mnt[128]; unsigned long long total, used;
    if (sscanf(out, "%127s %llu %llu %lld", fs, &total, &used, &avail) >= 4) printf("%s: %lld bytes free on /\n", host, avail);
    else printf("%s: %s", host, jb_trim(out));   // ssh error or unexpected format
    return 0;
}
