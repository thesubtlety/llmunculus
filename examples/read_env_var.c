// q: what is the value of a variable in a config or env file
#include <jb.h>
int main(void) {
    char *val = jb_conf_get("~/.itsvcs", "ITSHARE_ENDPOINT");   // handles export, =, :, quotes, and ~
    if (!val) { fprintf(stderr, "ITSHARE_ENDPOINT not found\n"); return 1; }
    printf("%s\n", val);
    return 0;
}
