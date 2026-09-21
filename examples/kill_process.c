// q: stop a process by name
#include <jb.h>
#include <signal.h>
int main(void) {
    const char *name = "example-daemon";
    char cmd[256]; snprintf(cmd, sizeof cmd, "pgrep -x %s 2>/dev/null", name);
    char *pids = jb_run(cmd); if (!pids) return 1;
    int n, killed = 0; char **v = jb_lines(pids, &n);
    for (int i = 0; i < n; i++) { long pid = strtol(v[i], NULL, 10); if (pid > 1 && kill((pid_t)pid, SIGTERM) == 0) killed++; }
    printf("sent SIGTERM to %d %s process(es)\n", killed, name);
    return 0;
}
