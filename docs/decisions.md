# decisions

| topic | choice | why |
|---|---|---|
| model | Qwen2.5-Coder-3B-Instruct, GGUF | best small coder. Fits 4 GB at any quant |
| quant | Q6_K | M8: Q8_0 same accuracy, 38% slower. Q4_0 slower through retries. accuracy is bounded by meaning errors, not bits |
| context | 8192, code steps up to 800 tokens | room for a real program. KV at 8k is 300 MB |
| engine | llama.cpp behind a small C interface | works now. Swap for our own at M10 |
| model storage | zip member of the APE, page aligned, read by seeking our own exe | never touches disk. one file. see 06-embed.md |
| compiler | libtcc, embedded | 200 KB. No host toolchain. tcc_run means no linker |
| run mode | child process runs the C code | a crash in model code must not kill the agent |
| child spawn | posix_spawn of our own exe with --run | no fork on Windows. cosmo has posix_spawn everywhere |
| libc headers for tcc | cosmo's own, copied. force-include normalize.inc and a dump of gcc's type macros | no hand-written libc. same struct layouts on every OS |
| model API surface | src/syms.h, about 200 functions, plus generated constants | tcc has no dynamic linker. unnamed means unreachable |
| IFUNC symbols | wrapped, never address-taken. make check-syms guards it | taking an IFUNC address breaks it for the whole cosmo binary |
| tcc runtime objects | cross-built at build time with host tcc per arch | tcc_run needs runmain.o. must be tcc output for the target |
| libc | Cosmopolitan (cosmocc), APE binary | one file for Linux, Mac, Windows. llamafile proves it |
| harness language | C | matches the theme. llama.cpp and tcc both have C APIs |
| protocol | terse text, grammar forced | small models fail at JSON. Grammar makes parse failures impossible |
| steps | one generation per step, each with its own grammar | short outputs. Easy to debug. Fewer wasted tokens |
| grammar order | sample first, check the pick, mask only on reject | grammar-first was 3x slower on a 151k vocab |
| system prompt | includes OS and arch from uname | without it the 3B wrote BSD code on Linux |
| context | n_ctx 8192, one live context per session | KV for 3B is about 36 KB per token. 8k is about 300 MB |
| retries | temperature +0.25 per retry, identical program refused | at 0.3 a retry is a replay |
| plan prompt | one example NEED, backticks banned by grammar | small models copy examples and ignore prohibitions |
| answer | one line | given slots the model fills every slot with a variant |
| facts | 80 chars, answer only | a fact rides in every later prompt |
| wrong includes | alias headers, #error hints for foreign APIs | tcc's own error teaches the model. no header map in the prompt |
| function table | about 340 calls. mutation and spawn calls are in it | the table is the surface; --read-only wraps every writable entry point (M9) |
| between tasks | KV rewound to the system prompt. facts and a short Q/A history travel as text | re-feeding 200 tokens beats an unbounded context |
| NONE in plan | offered only when facts exist | an informed shortcut, not a lazy one. M2 showed the difference |
| failed question | adds no fact | noise in facts compounds through squash and every later prompt |
| run memory | CONFIG_RUNMEM_RO=1 everywhere. `JB_TCCMEM=dual` for the two-view layout at runtime | Apple refuses rwx pages, hardened Linux may too. the retry needs no rebuild |
| tcc patch | one, `patches/tcc-dual-mem.patch`, 12 lines | makes tcc's SELinux layout a runtime choice. applied by make, idempotent |
| self-test | `--selftest` prints PASS/FAIL per piece | a fresh machine is one copy and one command away from a diagnosis |
| facts from output | exit 0 and short text output becomes the fact verbatim. no model in between | a 3B rejected `burn` as "not a valid username" and turned `fridg` into `fridgid`. values must not pass through prose |
| empty output | automatic fix request, no verdict asked | the model once accepted an empty line as a fact |
| answer check | any fact value missing from the answer is printed under it | the prose step paraphrases. the footer shows what was measured |
| timeout | -t flag, default 5 s, one automatic retry at 4x | a big tree walk is slow and still right |
| model precedence | command line, $JB_MODEL, model.gguf beside the file, embedded | an on-disk model costs nothing, the embedded one is never paged in |
| fixed-purpose builds | TASK= and SYSTEM= embed task.txt and system.txt | same binary, one more zip member |
| actions | DO steps, no gate, `--confirm` opt-in | the tool runs from cron |
| commands | `system` and `popen` allowed, C calls preferred | an IT tool needs systemctl and docker. C calls port to all three OSes, commands do not |
| scope | system prompt: look things up, files, compute, services, commands | the first one read as a hardware inventory tool |
| one step first | the whole task as a single NEED or DO, plan only as fallback | plan padding was half the eval's wall time and split compound actions |
| include repair | a missing libc header is prepended without asking | the most common compile error, and the model repeats it |
| read-only mode | wrappers in src/readonly.c replace fopen, open, write and the mutating and spawning calls | a cron inspection job cannot change anything even if the model tries. wrappers, not enforcement |
| no verdict without output | a timeout or crash goes straight to a fix request | a model asked anyway answered FACT: 200 after two timeouts |
| thinking models | detected by `<think>` tokens in the vocab, replies opened with the model's own empty think block | its non-thinking mode, not a fight with the grammar. bounded thinking is a later experiment |
| special tokens | banned at sampling inside replies, except end-of-generation | structure is not text. user-defined specials slipped through the grammar |
| trailing newlines | optional in every grammar | a model that ends replies with end-of-turn padded to the cap otherwise |
| reporting | the tool sends, never the model. json, file, syslog, native http and https | the model cannot be trusted to write a transport each time |
| tls | mbedtls vendored and compiled in, jb_https for https, no curl | the toolchain ships no TLS. one function, verifies against the system CA bundle |
| verdicts | yes/no tasks get a one-word grammar-bound verdict and exit 2 on yes | monitors act on exit codes |
| windows depth | PowerShell first, six Win32 wrappers second, no COM | tcc code cannot call the Microsoft ABI. wrappers can. WMI is one popen away |
| helpers | jb.h, twelve static functions, shipped with the headers | scaffolding is where a 3B loses track. less of it |
| examples | one per code step, chosen by word overlap, placed before the task, no function list beside it | examples beat instructions. the function list was the attractor |
| explore | --explore runs profile/*.c, one probe per section, each branching on jb_os() | a profile asks known questions. deterministic beats the model. one file per section, per-platform |
| remote | --remote scp's the binary and runs it on the host; probes can also ssh out | the single portable binary is the feature. no install on the remote. transport overridable |
| sandbox | deferred. the read-only wrappers are the interim | Landlock under them is M10 |
| style | terse plain English everywhere | learning tool. Less to read |

## risks, open

| risk | note |
|---|---|
| llama.cpp load from memory | solved. `llama_model_load_from_file_ptr` with the FILE* seeked to the member. gguf offsets are absolute |
| /zip mmap copies | solved. headers go through /zip. the model is loaded by seeking a FILE* to its offset, llama maps the whole exe from 0. no patch |
| tcc on Apple Silicon | build sets CONFIG_RUNMEM_RO=1, rx code and rw data, never rwx. same layout as tcc's own Mac port. untested here |
| tcc headers | solved. cosmo's headers work under tcc with two force-includes. 171 files, under 1 MB |
| Windows spawn | no fork. Re-exec self with `--run`. Same path on all three OSes |
| cosmo plus llama.cpp | upstream built clean with cosmocc at df03399. No patches needed. Hand-written mk/llama.mk |
| CPU dispatch | cosmo builds one binary for many CPUs. Need runtime AVX2 / AVX512 pick. llamafile does this |
| speed | 3B on a 4-core box is 5 to 8 tok/s. A task with 5 programs is minutes. Keep every step short |
