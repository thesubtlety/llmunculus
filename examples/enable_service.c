// q: enable a service so it starts at boot
#include <jb.h>
int main(void) {
    char *out = jb_run("systemctl enable --now nginx 2>&1; systemctl is-enabled nginx 2>/dev/null");   // prints enabled on success
    if (!out) return 1;
    int n; char **v = jb_lines(out, &n);
    printf("nginx: %s\n", n ? v[n - 1] : "?");
    return n && !strcmp(v[n - 1], "enabled") ? 0 : 1;
}
