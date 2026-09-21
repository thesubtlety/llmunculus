// q: how many bytes is the file /etc/passwd
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>

int main() {
    struct stat sb;
    if (stat("/etc/passwd", &sb) == 0) {
        printf("%ld\n", sb.st_size);
    } else {
        perror("stat");
        return 1;
    }
    return 0;
}
