// the child: read C source from stdin, compile it in memory with libtcc, run main, exit with its code.
// tcc errors go to stderr. a crash in the model's code kills only this process.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libtcc.h"
#include "runner.h"
#include "embed.h"
#include <cosmo.h>

struct sym { const char *name; void *ptr; };
extern const struct sym jb_syms[], jb_consts[], jb_readonly[];

static int n_errors;
static void on_error(void *ud, const char *msg) { (void)ud; n_errors++; fprintf(stderr, "%s\n", msg); }

static char *read_all(FILE *f) {
    size_t cap = 1 << 16, n = 0; char *s = malloc(cap);
    for (size_t k; (k = fread(s + n, 1, cap - n - 1, f)) > 0;) { n += k; if (n + 1 >= cap) s = realloc(s, cap *= 2); }
    s[n] = 0; return s;
}

// where headers and tcc's runtime objects live. build/ now, /zip at M6.
static const char *root(void) { return jb_root(); }
#if defined(__x86_64__)
#define ARCH "x86_64"
#else
#define ARCH "arm64"
#endif

int child_main(void) {
    char *src = read_all(stdin);
    char p[1024];
    TCCState *s = tcc_new();
    tcc_set_error_func(s, NULL, on_error);
    snprintf(p, sizeof p, "-nostdlib -nostdinc -include %s/include/jb_predefs.h -include %s/include/libc/integral/normalize.inc", root(), root());
    tcc_set_options(s, p);
    snprintf(p, sizeof p, "%s/include", root());          tcc_add_sysinclude_path(s, p);
    snprintf(p, sizeof p, "%s/tcc/lib/%s", root(), ARCH); tcc_add_library_path(s, p);   // runmain.o
    tcc_set_output_type(s, TCC_OUTPUT_MEMORY);
    int ro = getenv("JB_READONLY") != NULL;
    for (const struct sym *q = jb_syms; q->name; q++) {
        int overridden = 0;   // tcc will not take a name twice: in read-only mode the wrapper is the only definition
        if (ro) for (const struct sym *w = jb_readonly; w->name; w++) if (!strcmp(w->name, q->name)) overridden = 1;
        if (!overridden) tcc_add_symbol(s, q->name, q->ptr);
    }
    for (const struct sym *q = jb_consts; q->name; q++) tcc_add_symbol(s, q->name, q->ptr);
    if (ro) for (const struct sym *q = jb_readonly; q->name; q++) tcc_add_symbol(s, q->name, q->ptr);
    if (tcc_compile_string(s, src) < 0) return 2;
    snprintf(p, sizeof p, "%s/tcc/lib/%s/libtcc1.a", root(), ARCH); tcc_add_file(s, p);
    char *av[] = { "prog", NULL };
    fflush(stdout);
    int before = n_errors;
    int rc = tcc_run(s, 1, av);
    fflush(stdout); fflush(stderr);
    // tcc_run also links. an undefined symbol surfaces here, not at compile. report it like a compile failure.
    if (rc == -1 && n_errors > before) { fprintf(stderr, "(that function is not available in this environment)\n"); return 2; }
    return rc;
}
