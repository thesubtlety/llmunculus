// q: on Windows, how many event log errors in the last day
#include <jb.h>
int main(void) {
    char *out = jb_run("powershell -NoProfile -Command \"(Get-WinEvent -FilterHashtable @{LogName='System';Level=2;StartTime=(Get-Date).AddDays(-1)} -ErrorAction SilentlyContinue).Count\"");
    if (!out) return 1;
    printf("%s\n", jb_trim(out));
    return 0;
}
