# 04 loop

Status: done. Two tasks end to end on the 3B. One fully right, one two thirds right. Logs in build/run3b*.log.

## what is it

`loop_task` in `src/loop.c`. One task in, one answer out. Between them, a fixed state machine:

```
PLAN                         model lists up to 3 NEEDs           grammar: plan
for each NEED:
  CODE                       model writes one program            grammar: code
  run_c                      child compiles and runs it
    compile error  -> FIX    errors go back, model rewrites       grammar: code
    ran            -> OBSERVE output goes back, model says FACT or FIX   grammar: observe
  up to 3 code generations per NEED
  FACT -> facts list. KV cache rewound to just after PLAN.
ANSWER                       task + facts -> short plain answer  grammar: answer
```

Every model call is `step()`: one user turn, one grammar-bound reply, one log line. Nothing else talks to the model.

## incremental turns

A chat model expects role markers around each message. `llama_chat_apply_template` renders a whole conversation, but re-rendering and re-feeding on every step would pay prompt eval for the entire history each time. So the engine learns the three delimiter strings once and feeds only what is new.

How it learns them: render a fake conversation with marker bytes, `user \1`, `assistant \2`, `user \3`. The render is

```
[system] U_PRE \1 U_POST \2 A_POST U_PRE \3 U_POST
```

`U_POST` is the text between \1 and \2. `U_PRE` is the longest common suffix of the text before \1 and the text before \3. `A_POST` is what is left between \2 and the second `U_PRE`. For Qwen that gives `<|im_start|>user\n`, `<|im_end|>\n<|im_start|>assistant\n`, `<|im_end|>\n`. Nothing in the code names Qwen. Another model's template gives other strings.

`engine_turn_user(text)` feeds `U_PRE text U_POST`. `engine_gen` produces the reply. `engine_turn_end` feeds `A_POST`. The generation stops on the end-of-turn token, which we never decode, so `A_POST` puts it back.

## compaction by rewind

Solving one NEED adds code, compiler errors, program output and the model's replies to the context. Once the FACT exists none of that matters. `solve()` records the KV position before its first CODE step and rewinds to it after the FACT. The fact itself travels in the next step's user message as a `Known facts:` list.

So the context after three NEEDs holds: system prompt, the PLAN exchange, and whatever the current NEED is doing. Not the history. On this box that is the difference between a step costing 10 seconds of prompt eval and 40.

## what goes back to the model

- Compiler errors: first 20 lines, at most 1500 bytes. tcc's error line names the symbol, which is usually all the model needs.
- Program output: first 40 lines, at most 1500 bytes. Exit code always. Signal or timeout as one sentence.
- Nothing else. No tcc warnings unless they come with an error.

## budgets

| knob | value | why |
|---|---|---|
| NEEDs per task | 3 | more is slower and tiny models plan badly past 3 |
| code generations per NEED | 3 | first try plus two fixes |
| run timeout | 5 s | a probe program that runs longer is wrong |
| output cap | 4 KB | the child never blocks, the model never floods |
| temperature | 0.3 | code wants little variance. seed changes per attempt so a retry is not a replay |
| max tokens per step | 400, code 800 | a step that needs more is the wrong step. programs got more room after M8 |

## results

3B Q6_K, this box. Six runs of the loop over an afternoon, each one changing the prompts. Final state:

| task | needs | right | wall | notes |
|---|---|---|---|---|
| how many CPUs and how much RAM | 3 | 3 of 3 | 109 s | every step first try. answer: 4 CPUs, 8127725568 bytes, plus free disk it asked on its own |
| free disk on / and which user am I | 3 | 2 of 3 | 151 s | disk right. user name lost to a 4-argument getpwuid_r that the retry did not fix |

About 10 s per model step. A task is 8 to 12 steps.

## what the runs taught, in order

1. **The fix loop works.** `info.nprocs` did not compile, the error went back, the model changed it to `info.procs` and it ran. First run, no prompt tuning.
2. **The compiler checks syntax, nothing checks meaning.** `procs` is the process count. The model reported 322 CPUs with full confidence and the observe step did not blink. The hint toward `sysconf` and `/proc` fixed this case. It does not fix the class. M8's eval measures it.
3. **A retry at low temperature is a replay.** The model wrote the same nonexistent struct field three times, byte for byte, with the error in front of it each time. Two fixes. Temperature rises 0.25 per retry. An identical program is refused without running, with a message that says so.
4. **Small models follow examples, not prohibitions.** "No commands" was ignored one run in three. One example line, `NEED: How many bytes are free on the / filesystem?`, fixed the plan on every run since. It also gets copied into the plan now and then. Cheap price.
5. **Backticks in the grammar.** Banning them from a NEED line stopped the model quoting shell commands. A grammar can shape habits, not only formats.
6. **Slots get filled.** Given 1 to 5 answer lines, the model wrote the same answer five ways joined by OR. Given 1 to 3, three ways. The answer grammar is one line now.
7. **Long instructions get parroted.** A 25-word FACT format instruction came back inside the FACT. Now: `FACT: <the answer only, no comment>`, and the line is capped at 80 chars because it travels in every later prompt.
8. **One wrong include cost a right answer.** `_SC_PHYS_PAGES * _SC_PAGESIZE` was correct. `#include <sys/sysconf.h>` does not exist. Rather than teach every model the header map, `tools/hdrs.py` ships alias headers for a dozen names small models reach for. For foreign APIs like `sys/sysctl.h` the alias is a `#error` with advice, so tcc's own error message teaches the model.
9. **A missing function looked like a runtime failure.** tcc links inside `tcc_run`, so an unknown symbol failed with exit 255 and no useful text. The model tried to add `-lutil`. Now the child reports it as a compile failure with "that function is not available", and the table grew by 80 common calls.
10. **A cut-off program must not end the task.** 400 tokens without a closing fence used to return an error from the loop. Now it is a failed attempt with "write a shorter program".

## what broke

- Missing `quiet` after an edit to engine.c. Restored.
- `-Wformat-truncation` on a 4 KB snprintf into an 8 KB buffer. Silenced, the sizes are fine.
- Background runs were killed by the harness for low memory, with 5.7 GB available. The model is a file-backed map and shows up as cache. Runs now go in the foreground.
- `gethostid` and `getprogname` are in cosmo's headers but not its library. Dropped.
- The link error text is `unresolved reference`, not `undefined symbol`. The hint matched nothing until fixed.

## addendum, after the first runs on other machines

Windows produced `The user name is fridgid.` for a user named `fridg`. The program had printed the right thing. The model changed it while restating the fact. Three rules followed, all in `solve()` and `loop_task()`:

1. Exit 0 and short text output: the output is the fact, verbatim. The model is not asked.
2. Empty output: an automatic fix request. The model once said FACT to an empty line.
3. After the answer, every fact value is checked against the answer text. Missing ones are printed under it.

The model still writes the prose. It no longer gets to touch the numbers.
