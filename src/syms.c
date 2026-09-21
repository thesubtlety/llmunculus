// the table of libc symbols handed to tcc. no libc headers here on purpose:
// every name is declared as a bare function so the linker resolves it without a prototype fight.
#define X(n) extern void n(void);
#include "syms.h"
SYMS(X)
#undef X

// cosmo implements some string functions as IFUNCs, resolved at startup to an AVX or plain version.
// taking the address of an IFUNC in a static cosmo binary breaks that resolution, for the whole binary.
// so these go through a plain wrapper. `make check-syms` fails if an IFUNC name ever lands in SYMS.
char *strcpy(char *, const char *);
char *strstr(const char *, const char *);
static char *w_strcpy(char *d, const char *s) { return strcpy(d, s); }
static char *w_strstr(const char *h, const char *n) { return strstr(h, n); }

struct sym { const char *name; void *ptr; };
#define X(n) { #n, (void *)n },
const struct sym jb_syms[] = { SYMS(X) { "strcpy", (void *)w_strcpy }, { "strstr", (void *)w_strstr }, { 0, 0 } };
#undef X
