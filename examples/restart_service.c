// q: restart a service
#include <jb.h>
int main(void) {
    char *out = jb_run("systemctl restart nginx 2>&1 && systemctl is-active nginx 2>/dev/null");   // prints exactly "active" on success
    if (!out) return 1;
    char *state = jb_trim(out);
    printf("nginx: %s\n", state);
    return strcmp(state, "active") == 0 ? 0 : 1;
}
