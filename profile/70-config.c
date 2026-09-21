// section: config. a few security-relevant settings
#include <jb.h>
int main(void) {
    const char *os = jb_os();
    jb_section("config");
    if (!strcmp(os, "linux") || !strcmp(os, "macos")) {
        char *rl = jb_conf_get("/etc/ssh/sshd_config", "PermitRootLogin"); jb_row("ssh root login", rl ? rl : "default");
    }
    if (!strcmp(os, "linux")) {
        char *fwd = jb_read_file("/proc/sys/net/ipv4/ip_forward"); if (fwd) jb_row("ip forwarding", jb_trim(fwd)[0] == '1' ? "on" : "off");
        char *fw = jb_first_line(jb_run("ufw status 2>/dev/null | head -1")); if (!fw || !*fw) fw = jb_first_line(jb_run("firewall-cmd --state 2>/dev/null"));
        jb_row("firewall", fw && *fw ? fw : "unknown");
    } else if (!strcmp(os, "macos")) {
        jb_row("firewall", jb_first_line(jb_run("defaults read /Library/Preferences/com.apple.alf globalstate 2>/dev/null")));
        jb_row("sip", jb_first_line(jb_run("csrutil status 2>/dev/null")));
    } else if (!strcmp(os, "windows")) {
        jb_row("firewall", jb_first_line(jb_run("powershell -NoProfile -Command \"(Get-NetFirewallProfile | Where-Object Enabled).Name -join ','\"")));
        jb_row("rdp", jb_first_line(jb_run("powershell -NoProfile -Command \"(Get-ItemProperty 'HKLM:\\System\\CurrentControlSet\\Control\\Terminal Server').fDenyTSConnections\"")));
    }
    return 0;
}
