// q: what is the sha256 checksum of a file
#include <jb.h>
int main(void) {
    char *out = jb_run("sha256sum /etc/hostname 2>/dev/null || shasum -a 256 /etc/hostname 2>/dev/null");   // linux, then mac
    if (!out || !*out) return 1;
    char sum[128]; sscanf(out, "%127s", sum);
    printf("%s\n", sum);
    return 0;
}
