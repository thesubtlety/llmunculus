// q: which files under a directory are world writable
#include <jb.h>
#include <ftw.h>
static int hits;
static int visit(const char *p, const struct stat *st, int flag, struct FTW *f) { (void)f; if (flag == FTW_F && (st->st_mode & 002)) { printf("%s %o\n", p, st->st_mode & 0777); hits++; } return 0; }
int main(void) {
    if (nftw("/etc", visit, 16, FTW_PHYS)) { perror("nftw"); return 1; }
    printf("%d world-writable files\n", hits);
    return 0;
}
