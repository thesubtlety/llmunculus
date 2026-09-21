// q: how many files are under a directory tree and how big are they in total
#include <jb.h>
#include <ftw.h>
static long long files, bytes;
static int visit(const char *p, const struct stat *st, int flag, struct FTW *f) { (void)p; (void)f; if (flag == FTW_F) { files++; bytes += st->st_size; } return 0; }
int main(void) {
    if (nftw("/etc", visit, 16, FTW_PHYS)) { perror("nftw"); return 1; }   // FTW_PHYS: do not follow symlinks
    printf("%lld files, %lld bytes\n", files, bytes);
    return 0;
}
