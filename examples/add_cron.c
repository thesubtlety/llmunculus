// q: add a cron job that runs every hour
#include <jb.h>
int main(void) {
    const char *line = "0 * * * * /usr/local/bin/llmunculus --report syslog \"is the free disk space on / below 10 percent\"";
    char *cur = jb_run("crontab -l 2>/dev/null"); if (!cur) cur = "";
    if (strstr(cur, line)) { printf("already present\n"); return 0; }
    char cmd[2048]; snprintf(cmd, sizeof cmd, "(crontab -l 2>/dev/null; echo '%s') | crontab -", line);   // keep what is there, add one line
    if (system(cmd)) { fprintf(stderr, "crontab failed\n"); return 1; }
    printf("added: %s\n", line);
    return 0;
}
