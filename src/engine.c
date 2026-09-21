#include "engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include "llama.h"

struct engine {
    struct llama_model   *model;
    const struct llama_vocab *vocab;
    struct llama_context *ctx;
    int pos;
    int last_gen;
    int last_rejects;
    struct llama_token_data *cur;  // scratch, one entry per vocab token
    unsigned char *banned;         // 1 for special tokens that are not end-of-generation. never sampled in a reply
    char u_pre[128], u_post[160], a_post[64];  // chat template pieces around a user turn and after an assistant turn
    int thinking;                              // vocab has <think>: a thinking model. we open each reply with an empty block
};

// a thinking model has <think> and </think> as single tokens. its non-thinking mode is an empty block at the start of the reply,
// spelled exactly as the model spells it when it decides to think nothing: <think>, newline, </think>, two newlines.
static int has_token(const struct llama_vocab *v, const char *text) {
    llama_token t[4];
    int n = llama_tokenize(v, text, strlen(text), t, 4, false, true);
    return n == 1 && (llama_vocab_get_attr(v, t[0]) & (LLAMA_TOKEN_ATTR_CONTROL | LLAMA_TOKEN_ATTR_USER_DEFINED));
}

static void quiet(enum ggml_log_level lvl, const char *txt, void *ud) {
    (void)ud;
    if (lvl >= GGML_LOG_LEVEL_ERROR) fputs(txt, stderr);
}

// render the template with marker bytes, then cut the pieces out around them.
// render = [sys] U_PRE \1 U_POST \2 A_POST U_PRE \3 U_POST
static void learn_template(engine *e) {
    const char *tmpl = llama_model_chat_template(e->model, NULL);
    struct llama_chat_message m3[] = { {"user", "\x01"}, {"assistant", "\x02"}, {"user", "\x03"} };
    char buf[1024];
    int n = llama_chat_apply_template(tmpl, m3, 3, true, buf, sizeof buf);
    if (n < 0 || n >= (int)sizeof buf) return;
    char *p1 = strchr(buf, 1), *p2 = strchr(buf, 2), *p3 = strchr(buf, 3);
    if (!p1 || !p2 || !p3) return;
    // U_PRE is the longest common suffix of the text before \1 and the text before \3
    int pre = 0;
    for (int len = 1; len <= p1 - buf && len <= p3 - (p2 + 1) && len < (int)sizeof e->u_pre; len++)
        if (!memcmp(p1 - len, p3 - len, len)) pre = len;
    memcpy(e->u_pre, p1 - pre, pre); e->u_pre[pre] = 0;
    int k = p2 - (p1 + 1); if (k >= (int)sizeof e->u_post) k = sizeof e->u_post - 1;
    memcpy(e->u_post, p1 + 1, k); e->u_post[k] = 0;
    k = (p3 - pre) - (p2 + 1); if (k < 0) k = 0; if (k >= (int)sizeof e->a_post) k = sizeof e->a_post - 1;
    memcpy(e->a_post, p2 + 1, k); e->a_post[k] = 0;
}

#include <cosmo.h>
#include "embed.h"

static engine *open_model(struct llama_model *m, int n_ctx, int n_threads);

engine *engine_open_file(const char *path, int n_ctx, int n_threads) {
    llama_log_set(quiet, NULL);
    llama_backend_init();
    struct llama_model_params mp = llama_model_default_params();
    return open_model(llama_model_load_from_file(path, mp), n_ctx, n_threads);
}

engine *engine_open_embedded(int n_ctx, int n_threads) {
    size_t off, size;
    if (embed_find("model.gguf", &off, &size)) return NULL;
    (void)size;
    FILE *f = fopen(GetProgramExecutableName(), "rb");
    if (!f) return NULL;
    llama_log_set(quiet, NULL);
    llama_backend_init();
    // the gguf reader starts at the current position and records absolute offsets. llama then mmaps the
    // whole executable from 0 and indexes with those offsets. file-backed, no copy, no alignment needed.
    fseek(f, (long)off, SEEK_SET);
    struct llama_model_params mp = llama_model_default_params();
    return open_model(llama_model_load_from_file_ptr(f, mp), n_ctx, n_threads);
}

static engine *open_model(struct llama_model *m, int n_ctx, int n_threads) {
    if (!m) return NULL;
    struct llama_context_params cp = llama_context_default_params();
    cp.n_ctx = n_ctx;
    cp.n_batch = 512;
    cp.n_threads = cp.n_threads_batch = n_threads;
    struct llama_context *c = llama_init_from_model(m, cp);
    if (!c) { llama_model_free(m); return NULL; }
    engine *e = calloc(1, sizeof *e);
    e->model = m; e->vocab = llama_model_get_vocab(m); e->ctx = c;
    int nv = llama_vocab_n_tokens(e->vocab);
    e->cur = malloc(nv * sizeof *e->cur);
    // special tokens are structure, not text. <think>, </think>, <|im_start|> and friends must not appear inside a reply.
    // control tokens used to stop generation; user-defined specials slipped through the grammar as plain characters.
    e->banned = calloc(nv, 1);
    for (int i = 0; i < nv; i++)
        if ((llama_vocab_get_attr(e->vocab, i) & (LLAMA_TOKEN_ATTR_CONTROL | LLAMA_TOKEN_ATTR_USER_DEFINED)) && !llama_vocab_is_eog(e->vocab, i))
            e->banned[i] = 1;
    learn_template(e);
    e->thinking = has_token(e->vocab, "<think>") && has_token(e->vocab, "</think>");
    if (e->thinking) strncat(e->u_post, "<think>\n</think>\n\n", sizeof e->u_post - strlen(e->u_post) - 1);
    if (getenv("JB_DEBUG_BAN")) {   // how does the assistant prefix plus prefill tokenize?
        llama_token t[64]; int k = llama_tokenize(e->vocab, e->u_post, strlen(e->u_post), t, 64, false, true);
        fprintf(stderr, "[u_post] %d tokens:", k); for (int i = 0; i < k; i++) fprintf(stderr, " %d", t[i]); fprintf(stderr, "\n");
    }
    return e;
}

void engine_close(engine *e) {
    if (!e) return;
    llama_free(e->ctx); llama_model_free(e->model); free(e->cur); free(e->banned); free(e);
}

int engine_chat(engine *e, const char *system, const char *user, char *out, size_t cap) {
    struct llama_chat_message msgs[2]; int n = 0;
    if (system) { msgs[n].role = "system"; msgs[n].content = system; n++; }
    msgs[n].role = "user"; msgs[n].content = user; n++;
    const char *tmpl = llama_model_chat_template(e->model, NULL);
    int len = llama_chat_apply_template(tmpl, msgs, n, true, out, cap);
    return (len < 0 || (size_t)len >= cap) ? -1 : len;
}

int engine_turn_user(engine *e, const char *text) {
    size_t n = strlen(e->u_pre) + strlen(text) + strlen(e->u_post) + 1;
    char *buf = malloc(n);
    snprintf(buf, n, "%s%s%s", e->u_pre, text, e->u_post);
    int rc = engine_feed(e, buf);
    free(buf);
    return rc;
}
int engine_turn_end(engine *e) { return engine_feed(e, e->a_post); }
const char *engine_template_piece(engine *e, int i) { return i == 0 ? e->u_pre : i == 1 ? e->u_post : e->a_post; }
int engine_thinking(engine *e) { return e->thinking; }

// decode one batch of tokens, in n_batch sized chunks
static int decode(engine *e, llama_token *t, int n) {
    for (int i = 0; i < n; i += 512) {
        int k = n - i < 512 ? n - i : 512;
        if (llama_decode(e->ctx, llama_batch_get_one(t + i, k))) return -1;
        e->pos += k;
    }
    return 0;
}

int engine_feed(engine *e, const char *text) {
    int len = strlen(text);
    int n = -llama_tokenize(e->vocab, text, len, NULL, 0, e->pos == 0, true);
    if (n <= 0) return -1;
    llama_token *t = malloc(n * sizeof *t);
    llama_tokenize(e->vocab, text, len, t, n, e->pos == 0, true);
    int rc = decode(e, t, n);
    free(t);
    return rc ? -1 : n;
}

// fill the candidate array from the last logits
static struct llama_token_data_array candidates(engine *e) {
    const float *logits = llama_get_logits_ith(e->ctx, -1);
    int n = llama_vocab_n_tokens(e->vocab);
    if (getenv("JB_DEBUG_BAN")) {   // which token did the model want most, and was it banned?
        int best = 0; for (int i = 1; i < n; i++) if (logits[i] > logits[best]) best = i;
        if (e->banned[best]) { char buf[64]; int k = llama_token_to_piece(e->vocab, best, buf, sizeof buf, 0, true); if (k < 0) k = 0; buf[k] = 0;
            fprintf(stderr, "[ban] wanted token %d '%s' eog=%d\n", best, buf, llama_vocab_is_eog(e->vocab, best)); }
    }
    for (int i = 0; i < n; i++) e->cur[i] = (struct llama_token_data){ i, e->banned[i] ? -INFINITY : logits[i], 0.0f };
    return (struct llama_token_data_array){ e->cur, n, -1, false };
}

// sample first, then ask the grammar. only on a reject do we mask the whole vocab.
// checking 151k tokens against the parser every step costs more than the model itself.
static llama_token pick(engine *e, struct llama_sampler *chain, struct llama_sampler *gram) {
    struct llama_token_data_array p = candidates(e);
    llama_sampler_apply(chain, &p);
    llama_token id = p.data[p.selected].id;
    if (gram) {
        struct llama_token_data one = { id, 1.0f, 0.0f };
        struct llama_token_data_array one_p = { &one, 1, -1, false };
        llama_sampler_apply(gram, &one_p);
        if (one.logit == -INFINITY) {
            e->last_rejects++;
            p = candidates(e);
            llama_sampler_apply(gram, &p);
            llama_sampler_apply(chain, &p);
            id = p.data[p.selected].id;
        }
        llama_sampler_accept(gram, id);
    }
    llama_sampler_accept(chain, id);
    return id;
}

int engine_gen(engine *e, const char *gbnf, float temp, unsigned seed, int max_tok, char *out, size_t cap) {
    struct llama_sampler *gram = NULL;
    if (gbnf) {
        gram = llama_sampler_init_grammar(e->vocab, gbnf, "root");
        if (!gram) { fprintf(stderr, "bad grammar\n"); return -1; }
    }
    struct llama_sampler *s = llama_sampler_chain_init(llama_sampler_chain_default_params());
    if (temp <= 0) {
        llama_sampler_chain_add(s, llama_sampler_init_greedy());
    } else {
        llama_sampler_chain_add(s, llama_sampler_init_min_p(0.05f, 1));
        llama_sampler_chain_add(s, llama_sampler_init_temp(temp));
        llama_sampler_chain_add(s, llama_sampler_init_dist(seed));
    }

    e->last_rejects = 0;
    size_t used = 0; int n = 0;
    for (; n < max_tok; n++) {
        llama_token id = pick(e, s, gram);
        if (getenv("JB_DEBUG_BAN")) fprintf(stderr, "[tok] %d\n", id);
        if (llama_vocab_is_eog(e->vocab, id)) break;
        // a control token like <|im_start|> is not text. the grammar sees its piece as plain chars, so stop here.
        if (llama_vocab_get_attr(e->vocab, id) & LLAMA_TOKEN_ATTR_CONTROL) break;
        char buf[256];
        int k = llama_token_to_piece(e->vocab, id, buf, sizeof buf, 0, true);
        if (k < 0 || used + k + 1 > cap) break;
        memcpy(out + used, buf, k); used += k;
        if (decode(e, &id, 1)) { used = (size_t)-1; break; }
    }
    if (used != (size_t)-1) out[used] = 0;
    e->last_gen = n;
    llama_sampler_free(s);
    if (gram) llama_sampler_free(gram);
    return used == (size_t)-1 ? -1 : (int)used;
}

int  engine_pos(engine *e) { return e->pos; }
int  engine_last_gen_tokens(engine *e) { return e->last_gen; }
int  engine_last_rejects(engine *e) { return e->last_rejects; }
void engine_rewind(engine *e, int pos) {
    if (pos >= e->pos) return;
    llama_memory_t mem = llama_get_memory(e->ctx);
    bool ok = llama_memory_seq_rm(mem, 0, pos, -1);
    llama_pos last = llama_memory_seq_pos_max(mem, 0);   // the cache's own idea of the last position
    if (!ok || last != pos - 1) fprintf(stderr, "engine_rewind: cache disagrees. ok=%d want last=%d got %d\n", ok, pos - 1, (int)last);
    e->pos = pos;
}

int engine_kv_pos(engine *e) { return (int)llama_memory_seq_pos_max(llama_get_memory(e->ctx), 0) + 1; }
