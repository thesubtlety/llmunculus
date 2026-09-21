# context

The scarce thing is tokens in the window. Not disk, not RAM.

## budget (n_ctx 8192, the default since M8)

| part | tokens |
|---|---|
| system prompt + examples | ~400 |
| facts, 1 line each | ~20 each |
| one C program | 150 to 400 |
| one program output, capped | ~200 |
| one turn of user text | ~50 |

## rules, as built

1. One llama context for the whole session. The system prompt is fed once. Done in M4 and M5.
2. After OBSERVE yields a FACT, rewind the KV to before CODE. The fact travels as text. Code and output are gone. Done in M4, `solve()`.
3. Program output goes in capped: first 40 lines, 1500 bytes. Compile errors: first 20 lines, 1500 bytes. Done in M4.
4. After the answer, rewind to the system prompt. The next task starts from 41 tokens. Facts and a short history travel as text. Done in M5.
5. When the facts text passes 1500 bytes, the model squashes it. Done in M5.
6. The engine checks every rewind against the cache's own position. Done in M5.

Measured: a three-task session on the 3B never passed 700 tokens of context.

## KV cache in numbers

Qwen2.5-3B: 36 layers, 2 KV heads, head dim 128, f16.
Per token: 2 (K and V) x 36 x 2 x 128 x 2 bytes = 36,864 bytes.
8192 tokens: about 300 MB. Fine.

## rewind

`engine_rewind(pos)` drops KV entries past pos. This is `llama_kv_cache_seq_rm` in llama.cpp. It is what makes step 2 cheap: the model never re-reads what we threw away.

## optional

Save the system prompt KV state once, load at start. Saves a few seconds per launch. This writes to disk, so it is opt-in only.
