# architecture

## flow

```
user prompt
  -> PLAN    model lists what it needs to learn        grammar: plan
  -> CODE    model writes one C program                grammar: code
  -> COMPILE child: tcc compiles                       fail -> FIX (max 3) -> CODE
  -> RUN     child runs it, timeout, output cap
  -> OBSERVE model turns output into one FACT line     grammar: observe
  -> more NEEDs? -> CODE   else -> ANSWER              grammar: answer
```

Facts live in a list. Each new step sees the facts, not the old code or output.

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
| report.c | the report document and its delivery |
| allhdrs.c | every header the model may include. input to tools/hdrs.py |
| tools/hdrs.py | copies the header closure, writes the constants table |
| ctx.c | transcript, facts, token budget, compaction |
| host.c | OS and arch string from uname, for the system prompt |
| embed.c | read our own exe as a zip. find a member's data offset. zip64 aware |
| selftest.c | `--selftest`: one PASS/FAIL line per piece, for fresh machines |
| tools/embed.py | add headers, tcc runtime and the aligned model to the APE |
| patches/ | tcc-dual-mem.patch, the only change to third-party code |
| include/ | copy of cosmo's headers the model may use, plus jb.h and jb_win.h, made by tools/hdrs.py |
| examples/ | example programs, `// q:` first line. the closest one goes in the code prompt |

## engine interface

```c
// engine.h
typedef struct engine engine;
engine *engine_open_file(const char *path, int n_ctx, int n_threads);   // M6 adds an in-memory open
int     engine_chat(engine *, const char *system, const char *user, char *out, size_t cap);
int     engine_feed(engine *, const char *text);          // tokenize, eval, extend KV
int     engine_gen(engine *, const char *gbnf, float temp, unsigned seed, int max_tok, char *out, size_t cap);
int     engine_pos(engine *);                             // tokens used
void    engine_rewind(engine *, int pos);                 // drop KV after pos
void    engine_close(engine *);
```

M10 replaces engine.c and keeps this header.

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
[ zip store: model.gguf  include/**  tcc/lib/x86_64/*  tcc/lib/arm64/* ]
```

Headers and tcc objects are read through `/zip/`, which copies, fine for small files. The model is found with `embed_find`, then a FILE* on our own exe is seeked to it and handed to llama.cpp, which maps the whole file from 0. File-backed, no copy. Member is stored and page aligned. `make build/justabuilder` produces the file.
