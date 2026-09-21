// q: how many lines does /etc/passwd have
#include <jb.h>
int main(void) {
    char *t = jb_read_file("/etc/passwd"); if (!t) return 1;
    int n; jb_lines(t, &n);
    printf("%d\n", n);
    return 0;
}
