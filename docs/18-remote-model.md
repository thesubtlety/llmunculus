# 18 remote model

Status: built and verified against Ollama- and OpenAI-shaped stub servers, including schema mode. No real model served over HTTP from this box yet; the first live run is the user's Ollama/org endpoint.

## what it is

A second model backend. Instead of running a local GGUF, the agent can call a chat endpoint over HTTP: Ollama, or an OpenAI-compatible server (llama.cpp's server, vLLM). Everything else — tcc, the helpers in `jb.h`, ssh, `--remote` — is unchanged. This only swaps the brain.

Selected by `JB_MODEL_URL` (or `--model-url`):

```
JB_MODEL_URL=http://it-server:11434 JB_MODEL_NAME=qwen3-coder:30b llmunculus-thin "is / over 90% full"
```

`JB_MODEL_API` picks the request shape: `ollama` (default), `openai`, `llamacpp`, `vllm`. `JB_MODEL_NAME` is the model to request (Ollama needs it; others default to `default`).

Precedence: a command-line `.gguf` or `--use-model` still wins (an explicit local choice). Otherwise `JB_MODEL_URL` beats `JB_MODEL`, the file beside the binary, the embedded model, and the discovered one.

## how it maps

The loop drives the engine in user/assistant turns (`engine_turn_user` → `engine_gen` → `engine_turn_end`) with `engine_pos`/`engine_rewind` as a bookmark. A local engine holds this in a KV cache. A chat endpoint holds nothing, so the http engine keeps the conversation as a **messages array** and resends it whole on every request.

| engine.h call | http engine |
|---|---|
| `engine_turn_user(t)` | append `{user, t}` |
| `engine_gen(g,…)` | POST the messages + constraint; stash the reply; return it |
| `engine_turn_end()` | append `{assistant, reply}` |
| `engine_pos()` | message count |
| `engine_rewind(pos)` | drop messages past `pos` |

`engine_pos`/`rewind` as a message count is exact: the loop only rewinds to `base` (right after the system prompt), and that is message 1. The seed (`engine_chat` then `engine_feed` in `jb.c`) stores the system message; on the http path `engine_chat` returns the system text verbatim and `engine_template_piece` returns a byte that never appears, so jb.c's "keep only the system part" cut is a no-op.

Cost: the whole conversation goes over the wire each step. On a LAN/Tailscale GPU that is nothing next to the ~35 s a local CPU gen takes.

`src/engine_http.c` holds the backend; `engine.c` routes every public call to it when `e->http` is set. Transport is `jb_http` (plaintext, for `http://localhost`) or `jb_https` (`src/tls.c`). Responses are parsed by pulling the string value of `content` after `"message"` — the same shape for Ollama (`message.content`) and OpenAI (`choices[0].message.content`).

## the constraint, by backend

The local path's reliability on a small model comes from GBNF: the sampler masks any token that cannot continue the grammar, forcing plan lines and a clean code block. A chat endpoint is text in, text out, so the grammar only survives if the endpoint accepts one.

| flavor | endpoint | constraint |
|---|---|---|
| `llamacpp` | `/v1/chat/completions` | `grammar` = the GBNF, verbatim |
| `vllm` | `/v1/chat/completions` | `guided_grammar` = the GBNF |
| `ollama` | `/api/chat` | a JSON Schema in `format` (schema mode), else schema-free |
| `openai` | `/v1/chat/completions` | a JSON Schema in `response_format`, else schema-free |

llama.cpp and vLLM take the GBNF as-is, so the grammar guarantees hold unchanged.

Ollama and a generic OpenAI endpoint have no GBNF, only JSON Schema. For the grammars whose structure matters — the plan (`NEED/DO`), the code block, the verdict, the observation, yes/no — the engine sends a schema and then **converts the JSON reply back into the exact line text the parsers in `parse.c` already read** (`DO: …`, a `\`\`\`c` fence, `FACT`/`FIX: …`, `yes`). The loop and the parsers never learn a remote model was used. This all lives in `engine_http.c`; the grammar is identified by pointer (`gbnf == G_CODE`). `G_LINES` (facts squash) and `G_ANSWER` (free text) stay schema-free: the first would corrupt the facts list if mis-shaped, the second is a plain sentence.

So: a capable model (a 30B, an org model) holds format even schema-free; a small model gets the schema as a hard constraint, close to the local GBNF guarantee. `JB_MODEL_SCHEMA=0` turns schema mode off and falls back to prompt-only.

## limits

- Data leaves the box: the host's questions and the generated-code context go to the endpoint. Fine over Tailscale to your own server; local-only mode never sends anything off-box.
- One endpoint, no failover or retry on a 5xx. `JB_DEBUG_HTTP=1` prints the status and body on a non-200.
- Schema mode assumes the endpoint preserves JSON property order (kind before text); Ollama and OpenAI structured outputs do.
- Verified with stub `/api/chat` and `/v1/chat/completions` servers: URL backend selected, valid JSON requests with the `grammar` or `format`/`response_format` field, the messages array growing across turns, every reply shape reformatted correctly (unit-tested: plans, NONE, code fences, FIX, observe, yes/no, escaped and `\u` text), and the full plan → code → tcc compile → run → fact → done loop. First real model-over-HTTP run is the user's.
