// q: send a JSON message to an HTTP endpoint
#include <jb.h>
int main(void) {
    char *code = jb_run("curl -s -o /dev/null -m 10 -w '%{http_code}' -X POST -H 'Content-Type: application/json' "
                        "-d '{\"host\":\"web01\",\"disk_free_pct\":9}' http://127.0.0.1:8090/hook 2>/dev/null");
    if (!code) return 1;
    printf("posted, http %s\n", jb_trim(code));   // 000 means unreachable
    return jb_number_in(code) / 100 == 2 ? 0 : 1;
}
