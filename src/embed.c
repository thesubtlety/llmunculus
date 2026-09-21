// read our own executable as a zip and locate a member's raw bytes. no decompression: members we care about are stored.
// zip layout: [local header][data] ... [central directory][zip64 eocd][zip64 locator][eocd]
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <cosmo.h>
#include "embed.h"

static uint16_t u16(const unsigned char *p) { return p[0] | p[1] << 8; }
static uint32_t u32(const unsigned char *p) { return u16(p) | (uint32_t)u16(p + 2) << 16; }
static uint64_t u64(const unsigned char *p) { return u32(p) | (uint64_t)u32(p + 4) << 32; }

int embed_present(void) { return access("/zip/include/jb_predefs.h", R_OK) == 0; }

static int dbg;   // JB_DEBUG_ZIP=1 prints where the walk stops
#define D(...) do { if (dbg) fprintf(stderr, "embed: " __VA_ARGS__); } while (0)

int embed_find(const char *name, size_t *off, size_t *size) { return embed_find_in(GetProgramExecutableName(), name, off, size); }

int embed_find_in(const char *path, const char *name, size_t *off, size_t *size) {
    dbg = getenv("JB_DEBUG_ZIP") != NULL;
    FILE *f = fopen(path, "rb");
    if (!f) { D("cannot open %s\n", path); return -1; }
    fseek(f, 0, SEEK_END);
    long long end = ftell(f);
    // end of central directory: last 22 bytes plus up to 64K of comment. scan back for its signature.
    long long tail_len = end < 66000 ? end : 66000;
    unsigned char *tail = malloc(tail_len);
    fseek(f, end - tail_len, SEEK_SET);
    if (fread(tail, 1, tail_len, f) != (size_t)tail_len) { D("tail read failed, end=%lld\n", end); free(tail); fclose(f); return -1; }
    long long e = -1;
    for (long long i = tail_len - 22; i >= 0; i--) if (u32(tail + i) == 0x06054b50) { e = i; break; }
    if (e < 0) { D("no eocd in last %lld bytes, end=%lld\n", tail_len, end); free(tail); fclose(f); return -1; }
    uint64_t n_entries = u16(tail + e + 10), cd_off = u32(tail + e + 16);
    D("eocd at %lld entries=%llu cd_off=%llu\n", end - tail_len + e, (unsigned long long)n_entries, (unsigned long long)cd_off);
    if (cd_off == 0xFFFFFFFF || n_entries == 0xFFFF) {           // zip64: locator sits just before the eocd
        long long loc = e - 20;
        if (loc < 0 || u32(tail + loc) != 0x07064b50) { free(tail); fclose(f); return -1; }
        uint64_t z64 = u64(tail + loc + 8);
        unsigned char rec[56];
        fseek(f, z64, SEEK_SET);
        if (fread(rec, 1, 56, f) != 56 || u32(rec) != 0x06064b50) { free(tail); fclose(f); return -1; }
        n_entries = u64(rec + 32); cd_off = u64(rec + 48);
    }
    free(tail);
    // walk the central directory
    int rc = -1;
    fseek(f, cd_off, SEEK_SET);
    for (uint64_t i = 0; i < n_entries; i++) {
        unsigned char h[46];
        if (fread(h, 1, 46, f) != 46 || u32(h) != 0x02014b50) { D("bad cd entry %llu at %ld\n", (unsigned long long)i, ftell(f)); break; }
        uint16_t n = u16(h + 28), m = u16(h + 30), k = u16(h + 32);
        uint64_t csize = u32(h + 20), lho = u32(h + 42);
        // the extra field holds our alignment padding, up to a page, plus zip64 records. size it from the header.
        char fname[512]; unsigned char *extra = malloc(m ? m : 1);
        if (n >= sizeof fname) { D("name too long in entry %llu\n", (unsigned long long)i); free(extra); break; }
        fread(fname, 1, n, f); fname[n] = 0;
        fread(extra, 1, m, f);
        fseek(f, k, SEEK_CUR);
        if (strcmp(fname, name)) { free(extra); continue; }
        // zip64 extra (id 1) carries the real size and offset when the 32-bit fields are saturated
        for (int p = 0; p + 4 <= m;) {
            uint16_t id = u16(extra + p), len = u16(extra + p + 2); int q = p + 4;
            if (id == 1) {
                if (u32(h + 24) == 0xFFFFFFFF) q += 8;                          // uncompressed size
                if (csize == 0xFFFFFFFF) { csize = u64(extra + q); q += 8; }
                if (lho == 0xFFFFFFFF) lho = u64(extra + q);
            }
            p += 4 + len;
        }
        free(extra);
        unsigned char lh[30];                                                    // local header gives the real name/extra lengths
        fseek(f, lho, SEEK_SET);
        if (fread(lh, 1, 30, f) != 30 || u32(lh) != 0x04034b50) { D("bad local header at %llu\n", (unsigned long long)lho); break; }
        *off = lho + 30 + u16(lh + 26) + u16(lh + 28);
        *size = csize;
        rc = 0;
        break;
    }
    fclose(f);
    return rc;
}

const char *jb_root(void) {
    static char dir[1024];
    const char *r = getenv("JB_ROOT");
    if (r) return r;
    if (embed_present()) return "/zip";
    snprintf(dir, sizeof dir, "%s", GetProgramExecutableName());
    char *slash = strrchr(dir, '/'); if (slash) *slash = 0; else strcpy(dir, ".");
    return dir;
}
