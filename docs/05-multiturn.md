# 05 multiturn

Status: done. Three-turn sessions on the 3B, all answers right, follow-up turn needs no program. Log in build/run3b8-session.log.

## what is it

`jb model.gguf` with no task is a REPL. The model loads once. Each line is a task for `loop_task`. Between tasks a `session` struct carries what was learned. `/facts`, `/ctx`, `/reset`, `/quit`.

## what carries between tasks, and how

Not the KV cache. Text.

| carried | form | why text |
|---|---|---|
| facts | `- <question> <answer>` lines, 4 KB cap | short, labeled, cheap to re-feed. the question is the label, so `- 4` becomes `- How many processors are on this machine? 4` |
| history | `Q: / A:` for the last few tasks, 2 KB ring | pronouns. "how much is that in GB" needs the previous answer |
| KV cache | nothing. rewound to the system prompt after every task | the model re-reads about 200 tokens of facts per step, which is 15 s on this box, against a context that would otherwise grow without bound |

The trade is prompt eval against context size. Facts are small, so re-feeding wins. If facts grew to thousands of tokens the trade would flip, and the squash step below is what keeps them from getting there.

## the informed NONE

In M2, offering DONE in the plan step made the model take it two runs in three. That was a lazy option with no basis. Now the plan step sees the facts list, and only when it exists may it answer NONE. Turn two of the test session, "how much is that RAM in GB", got NONE in 0.6 seconds and a correct answer 3.5 seconds later. No program, no compile, no run. The same question as a fresh task would have cost two minutes.

Rule of thumb from M2 and M5 together: never offer a small model a shortcut it cannot justify from what is in front of it.

## one program's result in the next

A common question: can a program use another's result? Yes, through facts, not a runtime pipe. A step's program prints its answer, that line becomes a fact verbatim, and every later step sees all facts in its prompt. So "read the endpoint from the config" in one step, then "check that endpoint" in the next: the second program is written by the model with the value already in front of it, baked in as a literal. It is fact-passing through context, not stdin-to-stdout between two live programs. When two things belong together, one program does both in one `main`, which is what read_and_send does. When they are separate steps, the fact carries the value across.

## squash

When the facts text passes 1500 bytes the model rewrites it under a `- line` grammar, told to merge lines about the same thing and keep every number. The result replaces the list only if it is shorter. Tested with the 0.5B and a 200-byte cap to force it. Fires, rewinds, replaces. With the 3B a real session would take about ten tasks to reach it.

## a failure is not a fact

First version recorded `could not answer: <question>` in the facts. Three failed questions became three noise lines, and the squash step happily merged noise. Now a failed question adds nothing. The answer step is told what is known, not what is not.

## verifying the rewind

The engine keeps its own token counter. Every `ctx=` number in the logs comes from it, not from llama.cpp. A rewind that silently failed would leave the old tokens in the cache, the model would keep attending to them, and my counter would report a small context while the real one grew. `engine_rewind` now asks the cache for its last position after the removal and complains if they disagree. `/ctx` prints both. They agree, 41 and 41, after every task.

## results

3B Q6_K, one session, three turns.

| turn | task | plan | programs | right | model time |
|---|---|---|---|---|---|
| 1 | how many CPUs and how much RAM | 3 NEEDs | 3 | yes, 4 CPUs, 7751 MB | ~120 s |
| 2 | how much is that RAM in GB | NONE | 0 | yes, 7.751 GB | 4 s |
| 3 | which user am I and what is my home directory | 3 NEEDs | 3 | yes, burn, /home/burn | ~160 s |

Peak context 593 tokens. Five turns under 8k is not close. The whole session ran at under 700.

One wrong fact slipped in: "free memory available" came back as 26.9 GB, which is the free disk. The model wrote a statvfs program for a memory question and the observe step accepted a number three times larger than the RAM it had just measured. Meaning, again. M8 counts these.

## what broke

- Failed questions polluted facts. Fixed.
- Facts without labels: `- 4`. The question is the label now.
- Answers rambled to the 300 char cap. 200 now. Still rambles sometimes.
