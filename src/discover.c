// where other tools keep models, and which one fits us. only GGUF with a chat template, sized for this box.
#include "discover.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <math.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <cosmo.h>
#include "ggml.h"
#include "gguf.h"

#define MAXC 64
static model_cand cands[MAXC];
static int ncands;

static const char *home(void) {
    const char *h = getenv("HOME");
    if (!h || !*h) h = getenv("USERPROFILE");
    return h && *h ? h : ".";
}

static void add_path(const char *path) {
    if (ncands >= MAXC) return;
    for (int i = 0; i < ncands; i++) if (!strcmp(cands[i].path, path)) return;
    snprintf(cands[ncands++].path, sizeof cands[0].path, "%s", path);
}

static int is_gguf(const char *path) {
    FILE *f = fopen(path, "rb"); if (!f) return 0;
    char m[4]; int ok = fread(m, 1, 4, f) == 4 && !memcmp(m, "GGUF", 4);
    fclose(f); return ok;
}

// bounded walk. any_ext: take files without a .gguf suffix too (ollama blobs), checked by magic later
static void walk(const char *dir, int depth, int any_ext) {
    DIR *d = opendir(dir); if (!d) return;
    struct dirent *e;
    while ((e = readdir(d)) && ncands < MAXC) {
        if (e->d_name[0] == '.') continue;
        char p[1200]; snprintf(p, sizeof p, "%s/%s", dir, e->d_name);
        struct stat st; if (stat(p, &st)) continue;
        if (S_ISDIR(st.st_mode)) { if (depth > 0) walk(p, depth - 1, any_ext); continue; }
        if (!S_ISREG(st.st_mode) || st.st_size < (100u << 20)) continue;
        size_t n = strlen(e->d_name);
        if ((n > 5 && !strcasecmp(e->d_name + n - 5, ".gguf")) || (any_ext && is_gguf(p))) add_path(p);
    }
    closedir(d);
}

static void under(const char *base, const char *rel, int depth, int any_ext) {
    if (!base || !*base) return;
    char p[1200]; snprintf(p, sizeof p, "%s%s%s", base, *rel ? "/" : "", rel);
    walk(p, depth, any_ext);
}

static void collect(void) {
    const char *h = home(), *env;
    // ollama: blobs are GGUF files named by digest, next to tokenizer and manifest blobs
    if ((env = getenv("OLLAMA_MODELS")) && *env) under(env, "blobs", 0, 1);
    under(h, ".ollama/models/blobs", 0, 1);
    if (!IsWindows()) under("/usr/share/ollama/.ollama/models", "blobs", 0, 1);
    // LM Studio: publisher/repo/file.gguf
    under(h, ".lmstudio/models", 3, 0);
    under(h, ".cache/lm-studio/models", 3, 0);
    // hugging face hub: models--org--repo/snapshots/<rev>/[sub/]file.gguf, symlinks into blobs
    if ((env = getenv("HF_HUB_CACHE")) && *env) under(env, "", 4, 0);
    else if ((env = getenv("HF_HOME")) && *env) under(env, "hub", 4, 0);
    else under(h, ".cache/huggingface/hub", 4, 0);
    // llama.cpp -hf downloads
    if ((env = getenv("LLAMA_CACHE")) && *env) under(env, "", 1, 0); else under(h, ".cache/llama.cpp", 1, 0);
    // GPT4All, Jan, and plain folders people drop files in
    under(h, ".local/share/nomic.ai/GPT4All", 0, 0);
    under(h, "Library/Application Support/nomic.ai/GPT4All", 0, 0);
    under(getenv("LOCALAPPDATA"), "nomic.ai/GPT4All", 0, 0);
    under(h, "jan/models", 2, 0);
    under(h, ".local/share/Jan/data/models", 2, 0);
    under(h, "models", 1, 0);
    under(h, "Downloads", 0, 0);
}

static double ram_gb(void) {
    long pages = sysconf(_SC_PHYS_PAGES), psz = sysconf(_SC_PAGESIZE);
    return pages > 0 && psz > 0 ? (double)pages * psz / 1e9 : 8.0;
}

static int has(const char *hay, const char *needle) { return strcasestr(hay, needle) != NULL; }

// read the header only. -1 means not for us: no chat template, an encoder or vision model, or too big for the box.
static int inspect(model_cand *c) {
    struct stat st; if (stat(c->path, &st)) return -1;
    c->gb = st.st_size / 1e9;
    if (c->gb > ram_gb() * 0.7 || has(c->path, "mmproj")) return -1;
    struct gguf_init_params ip = { .no_alloc = true, .ctx = NULL };
    struct gguf_context *g = gguf_init_from_file(c->path, ip); if (!g) return -1;
    int64_t k;
    const char *arch = (k = gguf_find_key(g, "general.architecture")) >= 0 ? gguf_get_val_str(g, k) : "";
    const char *name = (k = gguf_find_key(g, "general.name")) >= 0 ? gguf_get_val_str(g, k) : "";
    int chat = gguf_find_key(g, "tokenizer.chat_template") >= 0;
    snprintf(c->name, sizeof c->name, "%s", *name ? name : arch);
    int bad = !chat || has(arch, "bert") || has(arch, "clip") || has(arch, "t5") || has(arch, "whisper");
    gguf_free(g);
    if (bad) return -1;
    char both[1200]; snprintf(both, sizeof both, "%s %s", c->path, c->name);
    c->score = 0;
    if (has(both, "coder") || has(both, "code")) c->score += 4;
    if (has(both, "instruct") || has(both, "chat") || has(both, "-it")) c->score += 2;
    if (has(both, "qwen")) c->score += 1;
    if (c->gb >= 1.5 && c->gb <= 4.5) c->score += 2;          // the band the loop was tuned in
    else if (c->gb >= 0.7 && c->gb <= 6.0) c->score += 1;
    return 0;
}

static void silent(enum ggml_log_level l, const char *t, void *u) { (void)l; (void)t; (void)u; }

static int by_score(const void *a, const void *b) {
    const model_cand *x = a, *y = b;
    if (x->score != y->score) return y->score - x->score;
    return x->gb < y->gb ? 1 : x->gb > y->gb ? -1 : strcmp(x->path, y->path);   // ties: more bits
}

// scan, score, and remember the winner. a corrupt file in someone's cache is skipped quietly
int discover_models(model_cand *out, int cap) {
    ncands = 0;
    collect();
    ggml_log_set(silent, NULL);
    int n = 0;
    for (int i = 0; i < ncands; i++) if (inspect(&cands[i]) == 0) cands[n++] = cands[i];
    qsort(cands, n, sizeof cands[0], by_score);
    if (n) { unlink(discover_cache_path()); FILE *f = fopen(discover_cache_path(), "w"); if (f) { fprintf(f, "%s\n", cands[0].path); fclose(f); } }
    if (n > cap) n = cap;
    memcpy(out, cands, n * sizeof *out);
    return n;
}

// temp, named per user. runs are throwaway and so is the pick
const char *discover_cache_path(void) {
    static char p[1200];
    const char *t = getenv("TMPDIR");
    if (!t || !*t) t = getenv("TEMP");
    if (!t || !*t) t = getenv("TMP");
    if (!t || !*t) t = "/tmp";
    snprintf(p, sizeof p, "%s/llmunculus-model.%u", t, (unsigned)getuid());
    return p;
}

// temp is shared: only trust a file we own, so nobody else chooses the GGUF we parse
static int ours(const char *path) {
    struct stat st; return !stat(path, &st) && S_ISREG(st.st_mode) && (IsWindows() || st.st_uid == getuid());
}

int discover_pick(const char *x, char *out, size_t cap) {
    if (access(x, R_OK) == 0) { snprintf(out, cap, "%s", x); return 0; }
    model_cand c[MAXC]; int n = discover_models(c, MAXC);
    char *end; long i = strtol(x, &end, 10);
    if (*x && !*end) { if (i < 1 || i > n) return -1; snprintf(out, cap, "%s", c[i - 1].path); return 0; }
    for (int k = 0; k < n; k++) if (has(c[k].name, x) || has(c[k].path, x)) { snprintf(out, cap, "%s", c[k].path); return 0; }
    return -1;
}

const char *discover_best(void) {
    static char best[1024];
    FILE *f = ours(discover_cache_path()) ? fopen(discover_cache_path(), "r") : NULL;
    if (f) {
        int ok = fgets(best, sizeof best, f) != NULL; fclose(f);
        best[strcspn(best, "\r\n")] = 0;
        if (ok && *best && is_gguf(best)) return best;
    }
    model_cand c[1];
    if (discover_models(c, 1) < 1) return NULL;
    snprintf(best, sizeof best, "%s", c[0].path);
    return best;
}
