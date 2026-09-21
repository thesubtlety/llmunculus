// q: run a health check by curling a URL and report the HTTP status code
#include <jb.h>
int main(void) {
    char *code = jb_run("curl -s -o /dev/null -m 10 -w '%{http_code}' http://127.0.0.1:8080/health 2>/dev/null");   // 000 when unreachable
    if (!code) return 1;
    printf("%s\n", jb_trim(code));
    return 0;
}
