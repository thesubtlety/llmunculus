// q: delete files older than 30 days in a directory
#include <jb.h>
#include <time.h>
int main(void) {
    const char *dir = "/tmp/jb-old"; int days = 30, removed = 0;
    int n; char **e = jb_dir_list(dir, &n); if (!e) { printf("nothing to do: %s not found\n", dir); return 0; }
    time_t cutoff = time(NULL) - (time_t)days * 86400; char path[1024]; struct stat st;
    for (int i = 0; i < n; i++) {
        snprintf(path, sizeof path, "%s/%s", dir, e[i]);
        if (stat(path, &st) == 0 && S_ISREG(st.st_mode) && st.st_mtime < cutoff && unlink(path) == 0) removed++;
    }
    printf("removed %d files older than %d days from %s\n", removed, days, dir);
    return 0;
}
