// q: how many processes are in state zombie or sleeping
#include <jb.h>
int main(void) {
    int n; char **e = jb_dir_list("/proc", &n); if (!e) return 1;
    int zombie = 0, sleeping = 0; char path[64];
    for (int i = 0; i < n; i++) {
        if (e[i][0] < '0' || e[i][0] > '9') continue;
        snprintf(path, sizeof path, "/proc/%s/status", e[i]);
        char *st = jb_read_file(path); if (!st) continue;
        char *state = jb_kv(st, "State", ':');   // "Z (zombie)", "S (sleeping)"
        if (state && state[0] == 'Z') zombie++;
        if (state && state[0] == 'S') sleeping++;
    }
    printf("zombie %d sleeping %d\n", zombie, sleeping);
    return 0;
}
