// q: how many network interfaces does this machine have
#include <jb.h>
int main(void) {
    int n; char **e = jb_dir_list("/sys/class/net", &n);   // one entry per interface
    if (!e) return 1;
    printf("%d\n", n);
    return 0;
}
