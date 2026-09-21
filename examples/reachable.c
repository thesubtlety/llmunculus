// q: is a host reachable
#include <jb.h>
int main(void) {
    const char *host = "127.0.0.1";
    char cmd[256]; snprintf(cmd, sizeof cmd, "ping -c 1 -W 2 %s >/dev/null 2>&1; echo $?", host);   // 0 = replied. works without ping too: 127 then
    char *out = jb_run(cmd); if (!out) return 1;
    long long rc = jb_number_in(out);
    printf("%s reachable: %s\n", host, rc == 0 ? "yes" : rc == 127 ? "unknown, no ping command" : "no");
    return 0;
}
