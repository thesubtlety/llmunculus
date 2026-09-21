// section: identity. hostname, os, kernel, arch, current user, uptime, time
#include <jb.h>
#include <sys/utsname.h>
#include <time.h>
#include <pwd.h>
int main(void) {
    struct utsname u; uname(&u);
    const char *os = jb_os();
    jb_section("identity");
    jb_row("hostname", u.nodename);
    char *pretty = 0;
    if (!strcmp(os, "linux")) pretty = jb_conf_get("/etc/os-release", "PRETTY_NAME");
    else if (!strcmp(os, "macos")) pretty = jb_first_line(jb_run("sw_vers -productName; sw_vers -productVersion"));
    else if (!strcmp(os, "windows")) pretty = jb_first_line(jb_run("powershell -NoProfile -Command \"(Get-CimInstance Win32_OperatingSystem).Caption\""));
    jb_row("os", pretty ? pretty : u.sysname);
    jb_row("kernel", u.release);
    jb_row("arch", u.machine);
    struct passwd *pw = getpwuid(getuid());
    if (pw) jb_row("user", pw->pw_name); else jb_row("user", jb_first_line(jb_run("whoami")));
    if (!strcmp(os, "linux")) { char *up = jb_read_file("/proc/uptime"); if (up) { long long s = jb_number_in(up); printf("%-16s %lld days %lld hours\n", "uptime", s/86400, s%86400/3600); } }
    else if (!strcmp(os, "macos")) jb_row("boot time", jb_first_line(jb_run("sysctl -n kern.boottime")));
    else if (!strcmp(os, "windows")) jb_row("boot time", jb_first_line(jb_run("powershell -NoProfile -Command \"(Get-CimInstance Win32_OperatingSystem).LastBootUpTime\"")));
    time_t now = time(NULL); char ts[64]; strftime(ts, sizeof ts, "%Y-%m-%d %H:%M:%S %Z", localtime(&now)); jb_row("time", ts);
    return 0;
}
