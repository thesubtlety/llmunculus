// q: on Windows, which services are stopped
#include <jb.h>
#include <jb_win.h>
int main(void) {
    char *list = malloc(262144); if (jb_win_services(list, 262144)) { perror("jb_win_services"); return 1; }
    int n, c = 0; char **v = jb_lines(list, &n);
    for (int i = 0; i < n; i++) if (strstr(v[i], " stopped")) { printf("%s\n", v[i]); c++; }
    printf("%d stopped\n", c);
    return 0;
}
