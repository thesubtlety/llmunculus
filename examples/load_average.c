// q: what is the load average
#include <jb.h>
int main(void) {
    char *t = jb_read_file("/proc/loadavg"); if (!t) return 1;   // "0.33 0.40 0.36 1/320 344258"
    double a, b, c; if (sscanf(t, "%lf %lf %lf", &a, &b, &c) != 3) return 1;
    printf("1min %.2f 5min %.2f 15min %.2f\n", a, b, c);
    return 0;
}
