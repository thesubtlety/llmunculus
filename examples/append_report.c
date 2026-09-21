// q: append a line to a log or report file
#include <jb.h>
#include <time.h>
#include <sys/statvfs.h>
int main(void) {
    struct statvfs v; if (statvfs("/", &v)) return 1;
    char line[256], ts[64]; time_t t = time(NULL); strftime(ts, sizeof ts, "%Y-%m-%d %H:%M:%S", localtime(&t));
    snprintf(line, sizeof line, "%s free=%llu\n", ts, (unsigned long long)v.f_bavail * v.f_frsize);
    if (jb_write_file("report.log", line, 1) < 0) return 1;
    printf("appended to report.log: %s", line);
    return 0;
}
