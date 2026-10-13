// q: ssh to a host as a user and write a file in their home directory
#include <jb.h>
int main(void) {
    const char *userhost = "alice@host";   // who and where
    char *out = jb_ssh(userhost, "printf 'hello\n' > \"$HOME/greet.txt\" && wc -c < \"$HOME/greet.txt\"");
    if (!out) { fprintf(stderr, "ssh to %s failed\n", userhost); return 1; }
    long long n = jb_number_in(out);
    if (n <= 0) { fprintf(stderr, "write on %s failed: %s\n", userhost, out); return 1; }
    printf("wrote %lld bytes to greet.txt in %s home\n", n, userhost);
    return 0;
}
