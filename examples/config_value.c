// q: what does a config file like sshd_config say about a setting like PermitRootLogin
#include <jb.h>
int main(void) {
    char *t = jb_read_file("/etc/ssh/sshd_config"); if (!t) { perror("sshd_config"); return 1; }
    int n; char **v = jb_lines(t, &n);
    for (int i = 0; i < n; i++) {                                   // "PermitRootLogin no", comments start with #
        char *l = jb_trim(v[i]);
        if (!strncmp(l, "PermitRootLogin", 15)) { printf("PermitRootLogin = %s\n", jb_trim(l + 15)); return 0; }
    }
    printf("PermitRootLogin not set (default applies)\n");
    return 0;
}
