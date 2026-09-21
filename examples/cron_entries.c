// q: list the cron jobs that are configured
#include <jb.h>
int main(void) {
    char *mine = jb_run("crontab -l 2>/dev/null");
    int n = 0; if (mine) { char **v = jb_lines(mine, &n); for (int i = 0; i < n; i++) if (v[i][0] && v[i][0] != '#') printf("user: %s\n", v[i]); }
    int k; char **e = jb_dir_list("/etc/cron.d", &k);   // system jobs live here and in /etc/crontab
    if (e) for (int i = 0; i < k; i++) printf("system: /etc/cron.d/%s\n", e[i]);
    return 0;
}
