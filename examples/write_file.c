// q: create a directory and write a file into it
#include <jb.h>
int main(void) {
    if (jb_exists("jbtest") != 2 && mkdir("jbtest", 0755)) { perror("mkdir jbtest"); return 1; }
    if (jb_write_file("jbtest/hello.txt", "hello world\n", 0) < 0) { perror("write"); return 1; }
    printf("wrote jbtest/hello.txt, %lld bytes\n", jb_file_size("jbtest/hello.txt"));
    return 0;
}
