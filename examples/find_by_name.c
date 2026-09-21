// q: find files by name under a directory
#include <jb.h>
#include <ftw.h>
#include <fnmatch.h>
static const char *pattern = "*.conf"; static int hits;
static int visit(const char *p, const struct stat *st, int flag, struct FTW *f) { (void)st; if (flag == FTW_F && !fnmatch(pattern, p + f->base, 0)) { printf("%s\n", p); hits++; } return 0; }
int main(void) {
    if (nftw("/etc", visit, 16, FTW_PHYS)) { perror("nftw"); return 1; }
    printf("%d matches\n", hits);
    return 0;
}
