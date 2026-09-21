// q: change a file's permissions
#include <jb.h>
int main(void) {
    if (jb_exists("jbtest") != 2) mkdir("jbtest", 0755);
    const char *path = "jbtest/hostname.copy";
    if (chmod(path, 0600)) { perror("chmod"); return 1; }
    struct stat st; stat(path, &st);
    printf("%s is now %o\n", path, st.st_mode & 0777);
    return 0;
}
