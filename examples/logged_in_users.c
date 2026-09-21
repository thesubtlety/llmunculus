// q: who is logged in right now
#include <jb.h>
int main(void) {
    char *out = jb_run("who 2>/dev/null");   // one session per line: user tty time
    if (!out) return 1;
    int n; char **v = jb_lines(out, &n);
    for (int i = 0; i < n; i++) { char user[64]; if (sscanf(v[i], "%63s", user) == 1) printf("%s\n", user); }
    printf("%d sessions\n", n);
    return 0;
}
