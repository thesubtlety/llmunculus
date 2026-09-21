// q: on macOS, what is the hardware model and how much memory
#include <jb.h>
int main(void) {
    char *model = jb_run("sysctl -n hw.model 2>/dev/null"), *mem = jb_run("sysctl -n hw.memsize 2>/dev/null");   // sysctl is the /proc of macOS
    if (!model || !mem) return 1;
    printf("%s %s bytes\n", jb_trim(model), jb_trim(mem));
    return 0;
}
