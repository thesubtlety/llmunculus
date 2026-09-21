# architecture

## flow

```
user task
  -> PLAN1   restate the whole task as one step: NEED (learn) or DO (change/report)   grammar: plan-one
       solve it; got a fact -> ANSWER.  else fall back to:
  -> PLAN    up to 3 NEED/DO steps, NEEDs before DOs                                  grammar: plan
  for each step:
    -> CODE      one C program (the closest example goes in the prompt)              grammar: code
    -> COMPILE   child: tcc.  missing header -> auto-include (no model).  else FIX (<=3)
    -> RUN       child runs it: timeout (one 4x retry), output cap, crash contained
    -> FACT      exit 0 + short text output IS the fact, verbatim (no model)
                 long/other output -> OBSERVE turns it into a fact or a FIX          grammar: observe/verdict
  -> ANSWER   one sentence; any measured value missing from it is appended verbatim  grammar: answer
  -> VERDICT  yes/no tasks: one word + exit code (2 = yes)                           grammar: yes/no
```

Facts travel as text. After each task the KV cache rewinds to the system prompt; facts and a short history carry to the next. Deterministic paths (--explore, --report, --remote) skip the model entirely.

## parts

| file | job |
|---|---|
| jb.c | args, system prompt, `--run` dispatch to the child, one task or the REPL |
| loop.c | state machine above. `session` carries facts and history between tasks |
| engine.c | llama.cpp wrapper. load, feed, sample, grammar |
| grammar.c | GBNF strings, one per step |
| parse.c | pull NEED, FACT, code block out of text |
| runner.c | spawn self `--run`, pipe C source in, collect output, timeout, cap |
| tcc_child.c | libtcc: compile string, run main, exit with its code |
| syms.c, syms.h | libc functions the model may call. IFUNCs wrapped |
| readonly.c | wrappers that refuse writes, spawns and sends in --read-only |
| win32.c, jb_win.h | curated Win32 helpers, SysV outside, Microsoft ABI inside |
| report.c | the report document and its delivery (json/file/syslog/http) |
| tls.c | native HTTPS (jb_https) via embedded mbedtls, cert-verified. no curl |
| allhdrs.c | every header the model may include. input to tools/hdrs.py |
| tools/hdrs.py | copies the header closure, writes the constants table |
| host.c | OS and arch string from uname, for the system prompt |
| embed.c | read our own exe as a zip. find a member's data offset. zip64 aware |
| selftest.c | `--selftest`: one PASS/FAIL line per piece, for fresh machines |
| tools/embed.py | add headers, tcc runtime and the aligned model to the APE |
| patches/ | tcc-dual-mem.patch, the only change to third-party code |
| include/ | copy of cosmo's headers the model may use, plus jb.h and jb_win.h, made by tools/hdrs.py |
| examples/ | example programs, `// q:` first line. the closest one goes in the code prompt |
| profile/ | one cross-platform probe per --explore section, run in order |
| explore, remote | deterministic modes in jb.c: host profile, and copy-and-run over ssh |

## engine interface

```c
// engine.h
typedef struct engine engine;
engine *engine_open_file(const char *path, int n_ctx, int n_threads);
engine *engine_open_embedded(int n_ctx, int n_threads);   // the model inside our own exe
int     engine_chat(engine *, const char *system, const char *user, char *out, size_t cap);
int     engine_turn_user(engine *, const char *text);     // incremental chat turns, template-derived
int     engine_turn_end(engine *);
int     engine_feed(engine *, const char *text);          // tokenize, eval, extend KV
int     engine_gen(engine *, const char *gbnf, float temp, unsigned seed, int max_tok, char *out, size_t cap);
int     engine_pos(engine *);                             // tokens used (engine's count)
int     engine_kv_pos(engine *);                          // same, from llama's cache; must match
void    engine_rewind(engine *, int pos);                 // drop KV after pos
int     engine_thinking(engine *);                        // 1 if the model has <think> tokens
void    engine_close(engine *);
```

M11 (optional) replaces engine.c with our own forward pass and keeps this header.

## runner interface

```c
// runner.h
typedef struct { int status; char *out; char *err; int timed_out; } run_result;
int run_c(const char *src, int timeout_ms, size_t out_cap, run_result *r);
```

Child side: `tcc_new`, `-nostdlib -nostdinc`, force-include `jb_predefs.h` and `normalize.inc`, add our include dir and `tcc/lib/<arch>`, register `jb_syms` and `jb_consts`, `tcc_compile_string`, add `libtcc1.a`, `tcc_run`. Compile errors go to stderr, exit 2. Program exit code passes through. A crash is a signal.

## binary layout

```
[ APE executable ]
[ zip: model.gguf (page-aligned) | include/** | tcc/lib/<arch>/* | examples/** | profile/** ]
```

Headers and tcc objects are read through `/zip/`, which copies, fine for small files. The model is found with `embed_find`, then a FILE* on our own exe is seeked to it and handed to llama.cpp, which maps the whole file from 0. File-backed, no copy. Member is stored and page aligned. `make build/llmunculus` produces the file.
