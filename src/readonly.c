// read-only mode: JB_READONLY=1 in the child. anything that could change the machine fails with EPERM and a note.
// fopen and open keep working for reading, stdout and stderr keep working. this is a wrapper layer, not enforcement:
// M10 adds Landlock under it. registered after jb_syms, and tcc keeps the later definition.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <fcntl.h>

static int refuse(const char *what) { fprintf(stderr, "read-only mode: %s refused\n", what); errno = EPERM; return -1; }

static FILE *ro_fopen(const char *path, const char *mode) {
    if (strpbrk(mode, "wa+")) { refuse("fopen for writing"); return NULL; }
    return fopen(path, mode);
}
static int ro_open(const char *path, int flags, int perm) {
    if (flags & (O_WRONLY | O_RDWR | O_CREAT | O_TRUNC)) return refuse("open for writing");
    return open(path, flags, perm);
}
static long ro_write(int fd, const void *b, size_t n) { if (fd > 2) return refuse("write to a file"); return write(fd, b, n); }
static int   ro_no1(const char *a)                 { (void)a; return refuse("changing the machine"); }
static int   ro_no2(const char *a, const char *b)  { (void)a; (void)b; return refuse("changing the machine"); }
static int   ro_no_int(int a, int b)               { (void)a; (void)b; return refuse("kill"); }
static int   ro_system(const char *c)              { (void)c; return refuse("system"); }
static FILE *ro_popen(const char *c, const char *m){ (void)c; (void)m; refuse("popen"); return NULL; }
static int   ro_fork(void)                         { return refuse("fork"); }
static int   ro_exec(const char *p, ...)           { (void)p; return refuse("exec"); }
static int   ro_sock(int a, const void *b, int c)  { (void)a; (void)b; (void)c; return refuse("network send"); }
static void  ro_syslog(int p, const char *f, ...)  { (void)p; (void)f; refuse("syslog"); }
static int   ro_remove(const char *p)              { (void)p; return refuse("remove"); }
static FILE *ro_freopen(const char *p, const char *m, FILE *f) { (void)p; (void)f; if (strpbrk(m, "wa+")) { refuse("freopen for writing"); return NULL; } return freopen(p, m, f); }
static int   ro_openat(int d, const char *p, int flags, ...) { (void)d; (void)p; if (flags & (O_WRONLY | O_RDWR | O_CREAT | O_TRUNC)) return refuse("openat for writing"); return openat(d, p, flags); }
static FILE *ro_tmpfile(void)                      { refuse("tmpfile"); return NULL; }
static long  ro_pwrite(int fd, const void *b, size_t n, long off) { (void)b; (void)n; (void)off; if (fd > 2) return refuse("pwrite to a file"); return -1; }
static FILE *ro_fdopen(int fd, const char *m)      { if (strpbrk(m, "wa+")) { refuse("fdopen for writing"); return NULL; } return fdopen(fd, m); }

struct sym { const char *name; void *ptr; };
const struct sym jb_readonly[] = {
    { "fopen", (void *)ro_fopen }, { "open", (void *)ro_open }, { "write", (void *)ro_write },
    { "mkdir", (void *)ro_no2 }, { "rmdir", (void *)ro_no1 }, { "unlink", (void *)ro_no1 }, { "rename", (void *)ro_no2 },
    { "chmod", (void *)ro_no2 }, { "chown", (void *)ro_no2 }, { "truncate", (void *)ro_no2 }, { "ftruncate", (void *)ro_no_int },
    { "symlink", (void *)ro_no2 }, { "link", (void *)ro_no2 }, { "utimes", (void *)ro_no2 }, { "setenv", (void *)ro_no2 },
    { "kill", (void *)ro_no_int }, { "system", (void *)ro_system }, { "popen", (void *)ro_popen }, { "fork", (void *)ro_fork },
    { "execv", (void *)ro_exec }, { "execve", (void *)ro_exec }, { "execvp", (void *)ro_exec }, { "execl", (void *)ro_exec }, { "execlp", (void *)ro_exec },
    { "jb_win_service_control", (void *)ro_no2 }, { "connect", (void *)ro_sock }, { "send", (void *)ro_sock }, { "sendto", (void *)ro_sock }, { "syslog", (void *)ro_syslog },
    { "remove", (void *)ro_remove }, { "freopen", (void *)ro_freopen }, { "openat", (void *)ro_openat }, { "tmpfile", (void *)ro_tmpfile },
    { "pwrite", (void *)ro_pwrite }, { "fdopen", (void *)ro_fdopen },
    { 0, 0 } };
