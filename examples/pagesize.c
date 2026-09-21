// q: what is the memory page size in bytes
#include <stdio.h>
#include <unistd.h>

int main() {
    long pagesize = sysconf(_SC_PAGESIZE);
    printf("%ld\n", pagesize);
    return 0;
}
