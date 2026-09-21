// q: get the OS and kernel of a remote host over ssh
#include <jb.h>
int main(void) {
    const char *host = "user@203.0.113.10";
    char cmd[256]; snprintf(cmd, sizeof cmd, "ssh -o BatchMode=yes -o ConnectTimeout=8 %s 'uname -sr' 2>&1", host);   // BatchMode: fail fast if keys are not set up
    char *out = jb_run(cmd); if (!out) return 1;
    printf("%s: %s", host, jb_trim(out));
    return 0;
}
