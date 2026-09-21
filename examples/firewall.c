// q: is a firewall active and what does it allow
#include <jb.h>
int main(void) {
    char *out = jb_run("ufw status 2>/dev/null");                               // ubuntu
    if (!out || !*out) out = jb_run("firewall-cmd --state 2>/dev/null");       // fedora, rhel
    if (!out || !*out) out = jb_run("nft list ruleset 2>/dev/null | head -20"); // anything else
    if (!out || !*out) { printf("no firewall tool found\n"); return 0; }
    printf("%s\n", jb_trim(out));
    return 0;
}
