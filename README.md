# llmunculus

One portable executable with a tiny LLM inside. Give it a task in plain English;
it writes small C programs, compiles and runs them to learn what it needs, and
answers. It can inspect a host, do actions, and report the result. No runtime, no
install, one file that runs on Linux, macOS and Windows.

A learning project, built milestone by milestone. Each step is written up in
[`docs/`](docs/) with what it is, how it works, and what broke.

## What it does

```
llmunculus --explore                 # a concise host profile, no model needed
llmunculus "how much disk is free"   # ask; it writes a C probe and answers
llmunculus "append the load average to health.log"        # act
llmunculus --report syslog "is / over 90% full"           # report + exit code
llmunculus --remote user@host --explore                   # profile a box over ssh
```

Also: `--read-only` (refuse writes), `--json`, `--selftest`, a REPL with no args.
Flags and env vars: [`docs/usage.md`](docs/usage.md).

## Build

Needs the [cosmocc](https://cosmo.zip) toolchain and three vendored sources
(fetched, not committed). From a clean clone:

```sh
# 1. toolchain -> ~/.local/opt/cosmocc
mkdir -p ~/.local/opt/cosmocc && cd ~/.local/opt/cosmocc
curl -sSL -o c.zip https://cosmo.zip/pub/cosmocc/cosmocc.zip && unzip -q c.zip && rm c.zip
mkdir -p extra && curl -sSL -o extra/zip https://cosmo.zip/pub/cosmos/bin/zip && chmod +x extra/zip
cd -

# 2. dependencies -> third_party/
git clone --depth 1 https://github.com/ggml-org/llama.cpp third_party/llama.cpp
git clone --depth 1 https://repo.or.cz/tinycc.git         third_party/tinycc
curl -sSL https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-3.6.2/mbedtls-3.6.2.tar.bz2 \
  | tar xj && mv mbedtls-3.6.2 third_party/mbedtls

# 3. a model -> models/  (any GGUF; this is the tuned default, ~2.8 GB)
curl -sSL -o models/qwen2.5-coder-3b-instruct-q6_k.gguf \
  https://huggingface.co/Qwen/Qwen2.5-Coder-3B-Instruct-GGUF/resolve/main/qwen2.5-coder-3b-instruct-q6_k.gguf

# 4. build (llama.cpp is ~7 min the first time)
make build/llmunculus        # 2.8 GB, model inside
make build/llmunculus-thin   # 15 MB, bring your own model
```

Verify on any machine: copy the binary over and run `./llmunculus --selftest`.

## How it works

A fixed loop: plan the task into steps, write one C program per step under a
grammar the model cannot break, compile it with an embedded [tcc](https://repo.or.cz/tinycc.git),
run it in a child process with a timeout, turn its output into a fact, and
answer. Measured values never pass through the model's prose. The model, the C
headers, the compiler and the examples all live inside the one file, read by
memory-mapping the executable itself. Full story in [`docs/`](docs/), start with
[`docs/plan.md`](docs/plan.md).

## Status

Works on Linux (x86_64 and arm64), Windows x86_64, and macOS arm64, all via one
`--selftest`-passing binary. On the 20-task eval a 3B model answers most host
questions correctly on the first program; failures are the model misreading a
question, not the tooling. CPU-only, so it pins cores while generating. Some
branches (the macOS/Windows profile probes, the Win32 wrappers, the tcc dual
memory mode) are written but unrun on that hardware.

## License

This code is MIT (see [LICENSE](LICENSE)). The repository ships source only; it
does not bundle its dependencies.

A **built binary** statically links, and thus redistributing one carries the
terms of: llama.cpp (MIT), Mbed-TLS (Apache-2.0), TinyCC (LGPL-2.1, patched —
see [`patches/`](patches/); LGPL allows relinking against a modified libtcc),
and Cosmopolitan libc (ISC). An **embedded model** carries its own license
(Qwen2.5-Coder is Apache-2.0). Publishing source is unencumbered; publishing
binaries means honoring those.
