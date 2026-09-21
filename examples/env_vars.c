// q: what is the value of an environment variable like PATH
#include <jb.h>
int main(void) {
    const char *v = getenv("PATH");
    printf("PATH=%s\n", v ? v : "(unset)");
    return 0;
}
