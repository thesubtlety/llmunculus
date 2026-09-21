// q: which TCP ports are listening
#include <jb.h>
int main(void) {
    char *t = jb_read_file("/proc/net/tcp"); if (!t) return 1;
    int n; char **v = jb_lines(t, &n);
    for (int i = 1; i < n; i++) {                      // line 0 is the header
        char local[64], rem[64]; unsigned st;
        if (sscanf(v[i], "%*d: %63s %63s %x", local, rem, &st) == 3 && st == 0x0A)   // 0A = LISTEN
            printf("%ld\n", strtol(strrchr(local, ':') + 1, NULL, 16));               // port is hex after the colon
    }
    return 0;
}
