// section: users. human accounts with shell and dates
#include <jb.h>
static void unix_users(int mac) {
    char *pwf = mac ? jb_run("dscl . -list /Users UniqueID 2>/dev/null | awk '$2>=500 && $2<60000{print $1}'") : jb_read_file("/etc/passwd");
    if (!pwf) return; int n; char **v = jb_lines(pwf, &n);
    for (int i = 0; i < n; i++) {
        char name[64], home[256] = "?", shell[128] = "?"; int uid = -1;
        if (mac) { snprintf(name, sizeof name, "%s", jb_trim(v[i])); if (!*name) continue;
                   char c[256], *h; snprintf(c, sizeof c, "dscl . -read /Users/%s NFSHomeDirectory 2>/dev/null | awk '{print $2}'", name); h = jb_first_line(jb_run(c)); if (h) snprintf(home, sizeof home, "%s", h); }
        else { char *f[7] = {0}; int nf = 0; for (char *p = v[i]; nf < 7; ) { f[nf++] = p; char *cc = strchr(p, ':'); if (!cc) break; *cc = 0; p = cc + 1; } if (nf < 7) continue;
               uid = atoi(f[2]); snprintf(name, sizeof name, "%s", f[0]); snprintf(home, sizeof home, "%s", f[5]); snprintf(shell, sizeof shell, "%s", f[6]);
               size_t sl = strlen(shell); int login = sl >= 2 && !strcmp(shell + sl - 2, "sh"); if (uid < 1000 || uid >= 65000 || !login) continue; }
        char sc[400]; snprintf(sc, sizeof sc, "stat -c %%y '%s' 2>/dev/null || stat -f %%Sm '%s' 2>/dev/null", home, home);
        char *since = jb_first_line(jb_run(sc));
        printf("%-16s uid %-6d %-12s home since %s\n", name, uid, shell, since && *since ? since : "?");
    }
}
int main(void) {
    const char *os = jb_os();
    jb_section("users");
    if (!strcmp(os, "linux")) unix_users(0);
    else if (!strcmp(os, "macos")) unix_users(1);
    else if (!strcmp(os, "windows")) printf("%s", jb_run("powershell -NoProfile -Command \"Get-LocalUser | Where-Object Enabled | ForEach-Object { '{0} lastlogon {1}' -f $_.Name,$_.LastLogon }\""));
    return 0;
}
