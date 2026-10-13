# 17 discover

Use the model the user already has. The full build is 2.8 GB because the model is inside. Most people who would run this tool already have a GGUF on disk from ollama, LM Studio, the Hugging Face cache or llama.cpp. The thin build (15 MB) can now find one and use it.

## what it does

Model precedence, in order: `.gguf` on the command line, `$JB_MODEL`, `model.gguf` beside the executable, the one inside the executable, then a scan. The scan is the last resort, so a full build never changes behavior.

The scan looks in the places each tool keeps its files, from `$HOME` (or `%USERPROFILE%`):

| tool | where | note |
|---|---|---|
| ollama | `.ollama/models/blobs/sha256-*`, `$OLLAMA_MODELS`, `/usr/share/ollama/.ollama/models` | blobs have no extension. the GGUF ones start with the bytes `GGUF` |
| LM Studio | `.lmstudio/models/<publisher>/<repo>/*.gguf`, older `.cache/lm-studio/models` | |
| Hugging Face | `.cache/huggingface/hub/models--*/snapshots/*/**.gguf`, `$HF_HOME`, `$HF_HUB_CACHE` | files are symlinks into `blobs/`. stat follows them |
| llama.cpp | `.cache/llama.cpp/*.gguf`, `$LLAMA_CACHE` | `-hf` downloads |
| GPT4All | `.local/share/nomic.ai/GPT4All`, mac `Library/Application Support/nomic.ai/GPT4All`, win `%LOCALAPPDATA%\nomic.ai\GPT4All` | |
| Jan | `jan/models`, `.local/share/Jan/data/models` | |
| by hand | `~/models`, `~/Downloads` | top level only |

Each candidate gets its header read with llama.cpp's gguf reader, `no_alloc`, so only metadata is touched. About 50 ms per file. Rejected: no `tokenizer.chat_template` (a base model, the loop needs chat turns), encoder or vision architectures (bert, clip, t5, whisper, mmproj), under 100 MB, or bigger than 70% of physical RAM.

Score: coder +4, instruct or chat +2, qwen +1, 1.5 to 4.5 GB +2 (the band the prompts were tuned in), 0.7 to 6 GB +1. Ties go to the larger file. On the fixture:

```
 9   3.6 GB  Qwen2.5 Coder 3B Instruct     ~/Downloads/qwen2.5-coder-3b-instruct-q8_0.gguf
 9   2.0 GB  Qwen2.5 Coder 3B Instruct     ~/.cache/huggingface/hub/.../qwen2.5-coder-3b-instruct-q4_0.gguf
 7   0.7 GB  Qwen2.5 Coder 0.5B Instruct   ~/.ollama/models/blobs/sha256-aaaa
 5   1.8 GB  Qwen3 1.7B Instruct           ~/.ollama/models/blobs/sha256-cccc
```

The pick is written to `$TMPDIR/llmunculus-model.<uid>` (`TEMP` or `TMP` on Windows, else `/tmp`). Later runs read that one line and skip the scan. If the file it names is gone or no longer a GGUF, the scan runs again. `--models` prints the table above and rewrites the cache. Delete the file to forget the pick.

## what we built

- `src/discover.c`: a bounded directory walk over the roots above, the header check, the score, the cache. 150 lines.
- `--models` in jb.c. `--selftest` uses the same fallback and names what it found.
- `pick_model` gained the last step and a clearer error when every step fails.

## what broke, what was decided

- **The cache lives in temp.** Runs are throwaway, so is the pick; a reboot costs one 0.4 s rescan. Temp is shared, so the file is named by uid, read only when owned by the current user, and unlinked before each write. The file it names is checked for the GGUF magic before use.
- **Names from the header, not from manifests.** ollama blobs are named by digest. The ollama manifest JSON maps tags to digests, but the GGUF header already carries `general.name`, and that works for every source. No JSON parsing.
- **Ollama blobs need a magic check.** The blob store holds the tokenizer, params and license as blobs too. A 200 MB file of zeros in the fixture was skipped by the magic check, not by size.
- **Embedded before discovered.** The embedded model is the tuned one. A found model may be worse, so a full build never prefers a found one. Users who want otherwise set `JB_MODEL`.
- **Thinking models score low on purpose.** Qwen3 1.7B is usable (see 05-multiturn.md and plan.md) but slower per step; a coder in the right size band outranks it.
- Scan time on the fixture: 0.4 s for four headers plus the walk. Not worth a cache by itself; the cache exists so a box with twenty models does not pay twenty header reads per run.

## not done

- No eval of discovered models beyond what M8 measured for the Qwen quants. A found Llama or Gemma will run but the prompts were tuned on Qwen2.5-Coder.
- Windows and mac paths are written from the tools' documentation, unrun on those hosts, like the rest of the Windows and mac branches.
- ollama can split a very large model's weights across blobs. Those are above the RAM cap on a small box anyway and are skipped.
