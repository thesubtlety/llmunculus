// q: which process is listening on a port, is port 22 in use
#include <jb.h>
int main(void) {
    int port = 22;
    char cmd[128]; snprintf(cmd, sizeof cmd, "ss -ltnp 'sport = :%d' 2>/dev/null | tail -n +2", port);   // users:(("sshd",pid=123,fd=3))
    char *out = jb_run(cmd); if (!out) return 1;
    if (!*out) { printf("nothing listening on %d\n", port); return 0; }
    char *u = strstr(out, "users:"); printf("port %d: %s\n", port, u ? jb_trim(u) : jb_trim(out));
    return 0;
}
