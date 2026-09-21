// q: copy a file to another place
#include <jb.h>
int main(void) {
    if (jb_exists("jbtest") != 2) mkdir("jbtest", 0755);
    char *t = jb_read_file("/etc/hostname"); if (!t) { perror("read"); return 1; }
    if (jb_write_file("jbtest/hostname.copy", t, 0) < 0) { perror("write"); return 1; }
    printf("copied /etc/hostname to jbtest/hostname.copy, %lld bytes\n", jb_file_size("jbtest/hostname.copy"));
    return 0;
}
