// q: which file in a directory was modified most recently
#include <jb.h>
#include <time.h>
int main(void) {
    const char *dir = "/var/log";
    int n; char **e = jb_dir_list(dir, &n); if (!e) return 1;
    char best[256] = ""; time_t best_t = 0; char path[1024]; struct stat st;
    for (int i = 0; i < n; i++) {
        snprintf(path, sizeof path, "%s/%s", dir, e[i]);
        if (stat(path, &st) == 0 && S_ISREG(st.st_mode) && st.st_mtime > best_t) { best_t = st.st_mtime; snprintf(best, sizeof best, "%s", e[i]); }
    }
    printf("%s\n", best);
    return 0;
}
