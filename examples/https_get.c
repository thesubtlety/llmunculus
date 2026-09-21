// q: fetch a url over https and report the status code
#include <jb.h>
int main(void) {
    char body[16384];
    int code = jb_https("GET", "https://example.com/health", NULL, NULL, body, sizeof body);
    if (code < 0) { fprintf(stderr, "tls request failed\n"); return 1; }
    printf("https status %d, %zu bytes\n", code, strlen(body));
    return code / 100 == 2 ? 0 : 1;
}
