// section: network. interfaces and ips, gateway, listening ports, established count, dns
#include <jb.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <netinet/in.h>
int main(void) {
    const char *os = jb_os();
    jb_section("network");
    if (!strcmp(os, "windows")) {
        printf("%s", jb_run("powershell -NoProfile -Command \"Get-NetIPAddress -AddressFamily IPv4 | ForEach-Object { '{0}={1}' -f $_.InterfaceAlias,$_.IPAddress }\" | tr '\\n' ' '")); printf("\n");
        jb_row("listening", jb_first_line(jb_run("powershell -NoProfile -Command \"(Get-NetTCPConnection -State Listen).LocalPort | Sort-Object -Unique\" | tr '\\n' ' '")));
        jb_row("dns", jb_first_line(jb_run("powershell -NoProfile -Command \"(Get-DnsClientServerAddress -AddressFamily IPv4).ServerAddresses\" | tr '\\n' ' '")));
        return 0;
    }
    // linux and macos: getifaddrs
    struct ifaddrs *ifa; char ips[512] = "";
    if (getifaddrs(&ifa) == 0) { for (struct ifaddrs *a = ifa; a; a = a->ifa_next) { if (!a->ifa_addr || a->ifa_addr->sa_family != AF_INET) continue;
        char ip[64]; inet_ntop(AF_INET, &((struct sockaddr_in *)a->ifa_addr)->sin_addr, ip, sizeof ip); char one[128]; snprintf(one, sizeof one, "%s=%s ", a->ifa_name, ip); strncat(ips, one, sizeof ips - strlen(ips) - 1); }
        freeifaddrs(ifa); jb_row("interfaces", ips); }
    if (!strcmp(os, "linux")) {
        char *r = jb_read_file("/proc/net/route"); if (r) { int n; char **v = jb_lines(r, &n); for (int i = 1; i < n; i++) { char f[32]; unsigned d, g; if (sscanf(v[i], "%31s %x %x", f, &d, &g) == 3 && d == 0) { struct in_addr a = { .s_addr = g }; char l[64]; snprintf(l, sizeof l, "%s via %s", inet_ntoa(a), f); jb_row("gateway", l); break; } } }
        char *lp = jb_run("ss -ltn 2>/dev/null | awk 'NR>1{print $4}' | sed 's/.*://' | sort -un | tr '\\n' ' '"); if (lp && *jb_trim(lp)) jb_row("listening ports", lp);
        char *es = jb_run("ss -tn state established 2>/dev/null | tail -n +2 | wc -l"); if (es) jb_row("connections", jb_trim(es));
        char *dns = jb_conf_get("/etc/resolv.conf", "nameserver"); if (dns) jb_row("dns", dns);
    } else if (!strcmp(os, "macos")) {
        jb_row("gateway", jb_first_line(jb_run("route -n get default 2>/dev/null | awk '/gateway/{print $2}'")));
        char *lp = jb_run("netstat -an -p tcp 2>/dev/null | awk '/LISTEN/{print $4}' | sed 's/.*\\.//' | sort -un | tr '\\n' ' '"); if (lp && *jb_trim(lp)) jb_row("listening ports", lp);
        char *es = jb_run("netstat -an -p tcp 2>/dev/null | grep -c ESTABLISHED"); if (es) jb_row("connections", jb_trim(es));
        jb_row("dns", jb_first_line(jb_run("scutil --dns 2>/dev/null | awk '/nameserver\\[0\\]/{print $3; exit}'")));
    }
    return 0;
}
