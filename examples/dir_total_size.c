// q: what is the total size in bytes of all files directly inside a directory
#include <jb.h>
int main(void) {
    const char *dir = "/var/log";
    int n; char **e = jb_dir_list(dir, &n); if (!e) return 1;
    long long total = 0; char path[1024];
    for (int i = 0; i < n; i++) {
        snprintf(path, sizeof path, "%s/%s", dir, e[i]);
        if (jb_exists(path) == 1) total += jb_file_size(path);   // 1 = regular file, 2 = directory
    }
    printf("%lld\n", total);
    return 0;
}
