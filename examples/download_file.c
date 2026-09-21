// q: download a file from a URL
#include <jb.h>
int main(void) {
    if (jb_exists("jbtest") != 2) mkdir("jbtest", 0755);
    if (system("curl -sSf -m 30 -o jbtest/robots.txt http://127.0.0.1:8090/robots.txt 2>/dev/null")) { fprintf(stderr, "download failed\n"); return 1; }
    printf("saved jbtest/robots.txt, %lld bytes\n", jb_file_size("jbtest/robots.txt"));
    return 0;
}
