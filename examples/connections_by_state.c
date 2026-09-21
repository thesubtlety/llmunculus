// q: how many TCP connections are there by state
#include <jb.h>
int main(void) {
    static const char *names[] = { "?", "established", "syn_sent", "syn_recv", "fin_wait1", "fin_wait2", "time_wait", "close", "close_wait", "last_ack", "listen", "closing" };
    int count[12] = { 0 };
    const char *files[] = { "/proc/net/tcp", "/proc/net/tcp6" };
    for (int f = 0; f < 2; f++) {
        char *t = jb_read_file(files[f]); if (!t) continue;
        int n; char **v = jb_lines(t, &n);
        for (int i = 1; i < n; i++) { unsigned st; if (sscanf(v[i], "%*d: %*s %*s %x", &st) == 1 && st < 12) count[st]++; }
    }
    for (int s = 1; s < 12; s++) if (count[s]) printf("%s %d\n", names[s], count[s]);
    return 0;
}
