// q: which files under a directory contain a word
#include <jb.h>
#include <ftw.h>
static const char *needle = "PermitRootLogin"; static int hits;
static int visit(const char *p, const struct stat *st, int flag, struct FTW *f) {
    (void)f; if (flag != FTW_F || st->st_size > 4 << 20) return 0;   // skip huge files
    char *t = jb_read_file(p); if (t && strstr(t, needle)) { printf("%s\n", p); hits++; } free(t); return 0;
}
int main(void) {
    if (nftw("/etc/ssh", visit, 16, FTW_PHYS)) { perror("nftw"); return 1; }
    printf("%d files contain %s\n", hits, needle);
    return 0;
}
