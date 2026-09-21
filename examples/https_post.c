// q: post a JSON status to an https endpoint
#include <jb.h>
int main(void) {
    char resp[4096];
    int code = jb_https("POST", "https://example.com/ingest", "{\"host\":\"web01\",\"disk_free_pct\":9}", "application/json", resp, sizeof resp);
    if (code < 0) { fprintf(stderr, "tls request failed\n"); return 1; }
    printf("posted, http %d\n", code);   // native TLS via jb_https, no curl
    return code / 100 == 2 ? 0 : 1;
}
