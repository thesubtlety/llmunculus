// q: what is the CPU model name
#include <jb.h>
int main(void) {
    char *t = jb_read_file("/proc/cpuinfo"); if (!t) return 1;
    char *v = jb_kv(t, "model name", ':');   // the line "model name : Intel..."
    printf("%s\n", v ? v : "unknown");
    return 0;
}
