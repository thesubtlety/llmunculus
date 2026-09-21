// q: which docker containers are running
#include <jb.h>
int main(void) {
    char *out = jb_run("docker ps --format '{{.Names}} {{.Status}}' 2>/dev/null");
    if (!out) return 1;
    if (!*out) { printf("none, or docker not available\n"); return 0; }
    int n; char **v = jb_lines(out, &n);
    for (int i = 0; i < n; i++) printf("%s\n", v[i]);
    printf("%d running\n", n);
    return 0;
}
