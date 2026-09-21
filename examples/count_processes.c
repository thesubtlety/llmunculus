// q: how many processes are running right now
#include <jb.h>
int main(void) {
    int n; char **e = jb_dir_list("/proc", &n);
    int procs = 0;
    for (int i = 0; i < n; i++) if (e[i][0] >= '0' && e[i][0] <= '9') procs++;   // numeric entries are pids
    printf("%d\n", procs);
    return 0;
}
