# 09 actions

Status: done. Files get written, reports get appended, and the single-step loop cut question tasks to a third of their time.

## what is it

Until M8 the loop could only ask. "write hello world to test.txt" produced three how-to questions and no file. Now a plan step may say `DO: <action>` as well as `NEED: <question>`. A DO gets a program that performs the action and prints one line saying what it did. Exit 0 plus that line is the fact, verbatim, labeled `did:`. A non-zero exit goes back to the model with stderr, through the same fix loop as a compile error.

Reporting a condition somewhere is the same path. A program that appends to a file, calls `syslog`, or sends to a socket is a DO. All three are in the function table.

## no gate

The tool runs from cron. A DO program runs as soon as it compiles, with the user's privileges. `--confirm` shows each action program on the terminal and asks first, and refuses when there is no terminal. Off by default, on purpose.

The function table is not a sandbox. `popen`, `fork` and `exec` were in it from M3, and `system` joined after M9: an IT tool needs `systemctl`, `journalctl`, `docker` and friends, which exist only as commands. The prompts ask for C calls where they exist, because those work on all three OSes, and commands where only a command has the answer. A read-only mode that hides the exec and mutation functions is M10's job.

## one step first

The eval showed plan padding as the top cost: three programs for one-step questions. Actions showed the other cost: a compound action split into fragments in the wrong order, `DO: write the df output` before `NEED: the date`, so the action never had its inputs.

So the loop now tries the whole task as one step first. The model restates it as a single NEED or DO, keeping every detail, and one program answers or does it. Only when that step yields nothing does the old plan run, up to three steps, NEEDs sorted before DOs so an action sees every fact gathered.

## deterministic repairs

Three things the loop now fixes without asking the model, because the model kept failing to:

- **A missing header.** `'errno' undeclared` means `#include <errno.h>`. A table of about seventy libc names maps to their header. The include is prepended and the program recompiled. It does not count as an attempt. In the first action run this turned a three-time failure into a pass.
- **Plan echo in the answer.** The answer grammar forbids a line starting with `DO:` or `NEED:`. The model found `NEED to` instead. Grammars shape form, not intent.

## results

3B Q6_K, this box, box quiet.

| task | path | programs | wall | outcome |
|---|---|---|---|---|
| write hello world to test.txt | plan, 3 steps | 3 | 213 s | test.txt holds `hello world`. include repair fired once |
| append date, free disk, load average to report.log | single step failed, plan of 2 | 3 | 664 s, partly under load | report.log has date, free bytes, and `inf` for the load average |

The `inf` is the 3B misreading `/proc/loadavg`. The mechanics did their job: NEED before DO, the action ran with the facts it needed, its output became the fact verbatim, and the file exists. The value is the model's.

Pure actions now answer as a `done.` block with one `did:` line per step and no model sentence. The sentence step restated the plan every time, once as `DO append` to slip past the grammar. When the output is a list of things done, a list is the honest answer.

The single-step loop on three eval questions:

| task | before, plan first | after, one step first |
|---|---|---|
| cpus | 95 to 215 s, 3 to 4 programs | 64 s, 1 program, right |
| user | 148 to 169 s, 4 to 5 programs | 51 s, 1 program, right |
| procs | 81 to 131 s, 3 programs, wrong | 61 s, 1 program, wrong the same way |

Two to three times faster, same answers. The meaning errors are untouched, as expected: they never came from the plan.

Where the single step fails: the restatement. Asked to restate an action as one line, the 3B wrote `NEED: Write a C program to ... Do: Compile and run the program`, a description of the tool rather than of the task, and that step produced nothing. The fallback plan then did the job. So a compound action costs one wasted step. A plain question does not.

## the shape of the loop after M9

```
PLAN1   one NEED or DO, the whole task            grammar: plan one
  solve it: CODE, run, (include repair | FIX)*, fact verbatim or verdict
  got a fact -> ANSWER
  else:
PLAN    up to 3 NEED or DO, NEEDs first          grammar: plan
  solve each
ANSWER  pure actions: done-block, no model
        otherwise: one sentence, values checked, missing ones listed under it
```

## what broke

- Every program failed with the same 113-byte error and the log showed only the size. The dev build looked for headers at the relative path `build/`, and the task ran from another directory. Root is now the executable's own directory. Compiler errors are logged in full with `-v`.
- The eval's `--limit 2` chunks overran the ten-minute cap on the slow quant. Chunks of two, patience.

## addendum, after a health-check run on another model

A 2B thinking model asked to curl a health endpoint showed four things at once, on a build from before the single-step loop.

1. Its socket program connected and read without sending a request. The server waited, the program timed out twice, and the model then said `FACT: 200`. Nothing had been observed. A timeout or crash no longer reaches a verdict step: it is an automatic fix request.
2. `system` was not in the table then. It is now, and the 3B on a local server did the whole task as one step with `curl -o /dev/null -s -w "%{http_code}"` through popen: `200`, verbatim, 104 s.
3. The plan padded the check with a disk question. Single step first removes that.
4. The answer came out as `<think>` and nothing else. A thinking model opens a thinking block, and the answer grammar allowed a leading `<`. It does not now. The plan, observe and code grammars already force a prefix, so they were never at risk. Letting the model think inside a bounded block is still on the later list, because this fix silences thinking rather than using it.

Read-only mode got one rule from its first end-to-end run: a refusal ends the step. The model had retried a refused write three times, and the refusal text says why retrying cannot help.

`--read-only` on "write hello world to test.txt": fopen refused, then open refused, then system refused, no file, and the answer said so. That is the behavior a cron inspection job wants.

## addendum, after a read-and-send task went wrong

"grep ~/.itsvcs for ITSHARE_ENDPOINT and send it to a host" on a weak 2B model exposed four things.

1. **~ never expanded.** The model tried to `fopen("~/.itsvcs")`, which does not work, and `fopen("/home/" + name)`, pointer arithmetic on a literal. `jb_read_file` and `jb_write_file` now expand a leading `~`, and `jb_conf_get(path, key)` reads a `KEY=VALUE` file directly, handling `export`, quotes and `~`.
2. **The example never reached the DO path.** The retrieval that fixed the eval was wired into the NEED prompt only. A DO, which is where sending and writing live, got none. So the model invented a socket that connected and then wrote the value to a local file while printing "sent". The example goes into both prompts now, and a DO needs it more: it shows `write(fd, ...)`, not just `connect`.
3. **Meta questions.** A plan step asked "What is required to make a small C program answer these questions?" and answered "libc". The plan prompt now says: ask only about the machine, never about the tool or C itself.
4. **Compound task, one DO.** "read X and send it" is one action, not a read NEED plus a send NEED that never connect. The plan prompts say a read-and-send is one DO, with that exact shape as the example.

After all four, Qwen2.5-Coder-3B does the task in one DO, one program, and a local receiver gets the value. The honest limit remains the model: a 2B non-coder will still struggle where a 3B coder does not.
