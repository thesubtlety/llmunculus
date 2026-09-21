// q: log a message to syslog
#include <jb.h>
#include <syslog.h>
int main(void) {
    openlog("justabuilder", LOG_PID, LOG_USER);
    syslog(LOG_WARNING, "disk on / is %d%% full", 91);
    closelog();
    printf("logged one warning to syslog\n");
    return 0;
}
