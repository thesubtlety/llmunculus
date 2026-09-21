// M1: load a GGUF, run one chat prompt, print tokens and speed.
// usage: infer model.gguf "prompt" [n_predict]
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "llama.h"
#include "ggml.h"

static void quiet(enum ggml_log_level lvl, const char *txt, void *ud) {
    (void)ud;
    if (lvl >= GGML_LOG_LEVEL_WARN) fputs(txt, stderr);
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s model.gguf prompt [n_predict]\n", argv[0]); return 1; }
    const char *path = argv[1], *user = argv[2];
    int n_predict = argc > 3 ? atoi(argv[3]) : 128;

    llama_log_set(quiet, NULL);
    llama_backend_init();

    struct llama_model_params mp = llama_model_default_params();
    struct llama_model *model = llama_model_load_from_file(path, mp);
    if (!model) { fprintf(stderr, "load failed: %s\n", path); return 1; }
    const struct llama_vocab *vocab = llama_model_get_vocab(model);

    // wrap the prompt in the model's chat template
    struct llama_chat_message msgs[] = {{"user", user}};
    const char *tmpl = llama_model_chat_template(model, NULL);
    char prompt[8192];
    int plen = llama_chat_apply_template(tmpl, msgs, 1, true, prompt, sizeof prompt);
    if (plen < 0 || plen >= (int)sizeof prompt) { fprintf(stderr, "template failed\n"); return 1; }

    // tokenize
    int n_prompt = -llama_tokenize(vocab, prompt, plen, NULL, 0, true, true);
    llama_token *toks = malloc(n_prompt * sizeof *toks);
    llama_tokenize(vocab, prompt, plen, toks, n_prompt, true, true);

    struct llama_context_params cp = llama_context_default_params();
    cp.n_ctx = n_prompt + n_predict;
    cp.n_batch = n_prompt;
    struct llama_context *ctx = llama_init_from_model(model, cp);
    if (!ctx) { fprintf(stderr, "context failed\n"); return 1; }

    struct llama_sampler *smpl = llama_sampler_chain_init(llama_sampler_chain_default_params());
    llama_sampler_chain_add(smpl, llama_sampler_init_greedy());

    // prompt eval
    int64_t t0 = ggml_time_us();
    struct llama_batch batch = llama_batch_get_one(toks, n_prompt);
    if (llama_decode(ctx, batch)) { fprintf(stderr, "decode failed\n"); return 1; }
    int64_t t1 = ggml_time_us();

    // generate
    int n_gen = 0;
    for (; n_gen < n_predict; n_gen++) {
        llama_token id = llama_sampler_sample(smpl, ctx, -1);
        if (llama_vocab_is_eog(vocab, id)) break;
        char buf[256];
        int n = llama_token_to_piece(vocab, id, buf, sizeof buf, 0, true);
        fwrite(buf, 1, n, stdout); fflush(stdout);
        batch = llama_batch_get_one(&id, 1);
        if (llama_decode(ctx, batch)) { fprintf(stderr, "decode failed\n"); return 1; }
    }
    int64_t t2 = ggml_time_us();

    printf("\n");
    fprintf(stderr, "prompt: %d tok in %.2fs = %.1f tok/s\n", n_prompt, (t1 - t0) / 1e6, n_prompt / ((t1 - t0) / 1e6));
    fprintf(stderr, "gen:    %d tok in %.2fs = %.1f tok/s\n", n_gen, (t2 - t1) / 1e6, n_gen / ((t2 - t1) / 1e6));

    llama_sampler_free(smpl); llama_free(ctx); llama_model_free(model); free(toks);
    return 0;
}
