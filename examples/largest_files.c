// q: what are the five largest files in a directory
#include <jb.h>
int main(void) {
    const char *dir = "/var/log";
    int n; char **e = jb_dir_list(dir, &n); if (!e) return 1;
    for (int round = 0; round < 5 && round < n; round++) {           // pick the largest remaining, five times
        int best = -1; long long best_size = -1; char path[1024];
        for (int i = 0; i < n; i++) {
            if (!e[i]) continue;
            snprintf(path, sizeof path, "%s/%s", dir, e[i]);
            long long sz = jb_exists(path) == 1 ? jb_file_size(path) : -1;
            if (sz > best_size) { best_size = sz; best = i; }
        }
        if (best < 0) break;
        printf("%lld %s\n", best_size, e[best]); e[best] = NULL;
    }
    return 0;
}
