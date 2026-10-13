// q: ssh to a host as a user and run a command, reporting what it printed
#include <jb.h>
int main(void) {
    const char *userhost = "alice@host";   // who and where
    char *out = jb_ssh(userhost, "uname -sr"); if (!out || !*out) { fprintf(stderr, "ssh to %s failed\n", userhost); return 1; }
    printf("%s: %s", userhost, jb_trim(out));
    return 0;
}
