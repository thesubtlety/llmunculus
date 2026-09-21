# 02 grammar

Status: done. 200 of 200 runs parsed. Numbers at the bottom.

## what is it

A GBNF grammar is a small BNF text. The sampler uses it to forbid any token that cannot continue a valid parse. The model still picks the words. It just cannot pick a shape we did not allow.

The grammars live in `src/grammar.c`. The engine applies one per `engine_gen` call. Each protocol step has its own.

## how it works, step by step

1. The model runs. We get one logit per vocab entry, about 151k numbers.
2. The grammar sampler holds a parser state. Really a set of possible states, since the grammar can be ambiguous.
3. For a candidate token, it takes the token's text and walks it through the parser, character by character. If any state survives, the token is allowed.
4. Allowed tokens keep their logit. Others get minus infinity. Softmax then gives them zero probability.
5. We sample. The chosen token is fed back into the parser, which advances.
6. When the grammar reaches its end, the only legal token left is end-of-text. Generation stops on its own.

Two details matter. A token can hold several characters and cross a rule boundary, so the walk in step 3 is per character, not per token. And a token can be a partial UTF-8 sequence, so the parser has to carry partial code points between tokens.

## the cost, and the fix

First version: grammar first in the sampler chain. Result on the 0.5B: 8.4 tok/s, against 24.8 with no grammar. Three times slower.

Cause: step 3 ran for all 151k tokens, every step. That is more work than the model's own forward pass on a small model.

Not the cause: the `{3,100}` bound on line length. I removed it to test. Speed did not change. Then it bit me for a different reason, see below.

Fix, copied from llama.cpp's own `common/sampling.cpp`: sample first with no grammar. Then ask the grammar about that one token. Only when it is rejected, redo the step with the grammar masking the whole vocab. This is `pick()` in `src/engine.c`.

Result: 23.4 tok/s with grammar. Same as without. The counter in the test driver shows why it works: the grammar overruled the model on 6 of 256 tokens. The model mostly wants to comply. The grammar only pays when it has to.

## what the grammar cannot do

- It cannot make the content good. The 0.5B produced five perfectly formed lines of advice instead of questions. The 3B, told "no commands", wrote `df -h` one run in three. Format is the grammar's job. Content is the prompt's and the model's.
- It cannot stop a line that never ends. `line ::= [^\n]+` let the 3B skip the final newline and ramble 127 tokens. It even emitted `<|im_start|>` as text, because the grammar sees a control token's piece as ordinary characters. Two fixes. Lines are bounded again, `{3,120}`. And `engine_gen` stops on any control token, not only end-of-text.
- It cannot know the OS. The 3B wrote `sysctl` code, which is BSD and macOS. Nothing in the prompt said Linux. `src/host.c` now puts `linux x86_64` from `uname` into the system prompt. After that it used `sysinfo`. Still wrong in detail, `struct sysinfo` has no `nprocs`, but wrong in a way the compiler will name.

## prompt lessons

- Offering `DONE` in the plan step made the 3B pick it two runs in three. Do not offer the lazy option. The plan step now only asks questions. Deciding "done" belongs to a later step.
- "Questions a small C program could answer. No commands." helped. Not fully. Temperature 0.6 gives variety, and some of that variety is disobedience. The M4 loop may want a lower temperature for plan and code, and a retry when a NEED looks like a shell command.
- The bound truncates. One good question got cut at 100 chars. 120 now. Watch for it.

## the grammars

```
# plan: 1 to 3 questions
root ::= need{1,3}
need ::= "NEED: " line "\n"
line ::= [^\n]{3,120}

# code: one fenced block. body may hold 1 or 2 backticks in a row, never 3.
root ::= "```c\n" body "```\n"
body ::= ( [^`] | "`" [^`] | "``" [^`] )*

# observe
root ::= ("FACT: " | "FIX: ") line "\n"

# answer
root ::= (line "\n"){1,5}
```

## numbers

0.5B Q8_0, temperature 0.6, seeds 1000 and up.

| step | runs | parsed | gen tok/s | grammar overrules |
|---|---|---|---|---|
| plan | 5 | 5 | 23.4 | 6 of 256 tokens |
| code | 5 | 5 | 23.4 | 15 of 194 tokens |
| plan | 100 | 100 | 24.1 | 237 of 4292 tokens |
| code | 100 | 100 | 24.8 | 319 of 4525 tokens |

3B Q6_K, 3 runs each: plan 3 of 3, code 3 of 3, 5.8 tok/s. Same speed as with no grammar.

## commands

```
make build/gramtest
./build/gramtest models/<gguf> plan "find out why the disk is filling up" 5
./build/gramtest models/<gguf> code "how many CPUs does this machine have" 5
./build/gramtest models/<gguf> free "any prompt" 3     # no grammar, for a baseline
```

## what broke

- Grammar-first sampling was 3x slower. Sample-then-check fixed it.
- Unbounded line rule let the model run on and leak a control token as text.
- No OS in the prompt gave BSD code on Linux.

## addendum, after the first thinking model

Qwen3-1.7B taught two grammar lessons in one afternoon.

**Special tokens are not text.** `<think>` and `</think>` are "user defined" specials, not control tokens, so the control-token stop ignored them and the grammar saw their pieces as ordinary characters. Lines came out as `NEED: How many CPUs?</think>`. Now the sampler bans every special token that is not end-of-generation before anything else looks at the logits. Structure tokens can no longer appear inside a reply on any model.

**A mandatory trailing newline is a trap.** In non-thinking mode Qwen3 ends a reply with end-of-turn directly, never with a newline. Every grammar demanded `"\n"` last, so the grammar rejected end-of-turn, the model's next choice was the banned `</think>`, and what remained was spaces. A plan line ran to 258 tokens of padding. Trailing newlines are optional in every grammar now, and newlines only separate items. The 3B got faster too: its plan step dropped from 64 to 40 seconds on the same task, because it had also been spending tokens to satisfy that newline.

The diagnostic that found both: `JB_DEBUG_BAN=1` prints the banned token the model most wanted at each step, and the token ids of the assistant prefix.
