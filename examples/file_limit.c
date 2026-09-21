// q: what is the open file descriptor limit for this process
#include <jb.h>
#include <sys/resource.h>
int main(void) {
    struct rlimit r; if (getrlimit(RLIMIT_NOFILE, &r)) return 1;
    printf("soft %llu hard %llu\n", (unsigned long long)r.rlim_cur, (unsigned long long)r.rlim_max);
    return 0;
}
