#pragma once
#include <stddef.h>
// find a stored member inside our own executable's zip. sets data offset and size. returns 0 if found.
int embed_find(const char *name, size_t *off, size_t *size);
int embed_find_in(const char *path, const char *name, size_t *off, size_t *size);
int embed_present(void);   // 1 if /zip holds our headers, so the child should use them
// where headers, tcc objects and examples live: $JB_ROOT, else /zip when embedded, else this executable's directory
const char *jb_root(void);
