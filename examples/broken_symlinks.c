// q: are there broken symlinks under a directory
#include <jb.h>
#include <ftw.h>
static int hits;
static int visit(const char *p, const struct stat *st, int flag, struct FTW *f) { (void)st; (void)f; if (flag == FTW_SLN) { printf("%s\n", p); hits++; } return 0; }   // SLN: a symlink pointing nowhere
int main(void) {
    if (nftw("/etc", visit, 16, FTW_PHYS)) { perror("nftw"); return 1; }
    printf("%d broken symlinks\n", hits);
    return 0;
}
