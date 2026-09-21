// q: is the sshd service running
#include <jb.h>
int main(void) {
    char *out = jb_run("systemctl is-active sshd 2>/dev/null");   // prints active, inactive, or failed
    if (!out) return 1;
    printf("%s\n", jb_trim(out));
    return 0;
}
