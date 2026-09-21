// thin wrapper over llama.cpp. M10 swaps the .c and keeps this file.
#pragma once
#include <stddef.h>

typedef struct engine engine;

engine *engine_open_file(const char *path, int n_ctx, int n_threads);
// the model stored inside our own executable, mapped in place. NULL if there is none.
engine *engine_open_embedded(int n_ctx, int n_threads);
void    engine_close(engine *);

// wrap a system+user message in the model's chat template. returns length or -1.
int engine_chat(engine *, const char *system, const char *user, char *out, size_t cap);

// incremental chat turns. feed one user message and open the assistant turn; close it after engine_gen.
// the delimiters come from the model's own template, so this is not tied to one model family.
int engine_turn_user(engine *, const char *text);
int engine_turn_end(engine *);
const char *engine_template_piece(engine *, int i); // 0 user prefix, 1 user suffix + assistant prefix, 2 assistant suffix. for logs
int engine_thinking(engine *);                       // 1 if the model has <think> tokens. replies are opened with an empty block

// tokenize text and push it through the model. extends the KV cache. returns tokens added or -1.
int engine_feed(engine *, const char *text);

// generate under a GBNF grammar (NULL = free text). generated tokens also enter the KV cache.
// returns bytes written, or -1.
int engine_gen(engine *, const char *gbnf, float temp, unsigned seed, int max_tok, char *out, size_t cap);

int  engine_pos(engine *);            // tokens in the KV cache, engine's count
int  engine_kv_pos(engine *);         // same, asked of llama.cpp's cache. must match
void engine_rewind(engine *, int pos); // drop everything after pos
int  engine_last_gen_tokens(engine *); // tokens produced by the last engine_gen
int  engine_last_rejects(engine *);    // times the grammar had to overrule the model in the last engine_gen
