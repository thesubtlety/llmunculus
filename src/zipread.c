// M0 test: read a file that lives inside our own executable.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

int main(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : "/zip/asset.txt";

    // 1. stdio works on /zip like any path
    FILE *f = fopen(path, "r");
    if (!f) { perror(path); return 1; }
    char line[128];
    while (fgets(line, sizeof line, f)) fputs(line, stdout);
    fclose(f);

    // 2. mmap works too. This is the path the model loader will take.
    int fd = open(path, O_RDONLY);
    struct stat st; fstat(fd, &st);
    void *p = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    printf("mmap %s: %ld bytes, first byte '%c'\n", path, (long)st.st_size, ((char *)p)[0]);
    munmap(p, st.st_size); close(fd);
    return 0;
}
