// q: how many users in /etc/passwd have a login shell ending in sh
#include <jb.h>
int main(void) {
    char *t = jb_read_file("/etc/passwd"); if (!t) return 1;
    int n, count = 0; char **v = jb_lines(t, &n);
    for (int i = 0; i < n; i++) {
        size_t len = strlen(v[i]);
        if (len >= 2 && !strcmp(v[i] + len - 2, "sh")) count++;   // the shell is the last field, so the line ends with it
    }
    printf("%d\n", count);
    return 0;
}
