# 08 eval

Status: done. Three quants, one run each, plus a prompt variant. Numbers below.

## what is it

`eval/tasks.txt` is twenty read-only questions about the machine. `tools/eval.py` asks the agent each one and grades the answer. Numbers come out per model.

Answers are machine-specific, so there are no stored answers. Each task carries a shell command that computes the expected value on the host at eval time. `nproc`, `df`, `stat`, `/proc/meminfo`. The command is the ground truth, the agent's C program is the thing under test.

## the tasks

| id | question | truth from | check |
|---|---|---|---|
| cpus | how many CPUs | nproc | exact |
| ram | RAM in bytes | /proc/meminfo | within 2% |
| swap | swap in bytes | /proc/meminfo | within 2% |
| diskfree | bytes free on / | df, and statfs f_bfree | within 3% |
| user | my user name | id -un | contains |
| home | my home directory | $HOME | contains |
| hostname | the hostname | hostname | contains |
| kernel | kernel version | uname -r | contains |
| uptime | seconds up | /proc/uptime | within 5% |
| procs | processes running | count of /proc/[0-9]* | within 15% |
| mounts | filesystems mounted | lines in /proc/mounts | within 15% |
| etcfiles | entries in /etc | ls -A /etc | within 5% |
| passwdsz | bytes in /etc/passwd | stat | exact |
| passwdln | lines in /etc/passwd | wc -l | exact |
| pagesize | page size | getconf | exact |
| pidmax | max pid | /proc/sys/kernel/pid_max | exact |
| osname | which distribution | /etc/os-release | contains |
| year | the year | date | contains |
| ifaces | network interfaces | ls /sys/class/net | exact |
| cpumodel | CPU model | /proc/cpuinfo | contains one word |

Tolerances are for things that move between the truth command and the agent's program: uptime, process count, free disk. A numeric check also tries the answer in KB, MB and GB, both 1000 and 1024 based, so "7751 MB" passes a bytes question.

Two lessons from writing the checks. `df` shows what a user can use, `statfs` shows what is free including the root reserve, and on this box those differ by 20%. "Free" is both, so both pass. And the eval harness is a Python program running an APE, which needs `sh` in front, same as `nm` did in M3.

## what is measured

Per task: pass or fail, model steps, programs run, compile errors, facts taken verbatim, wall seconds. The step log is saved beside the results for every task, because a FAIL line without the log is a number, and the log is the lesson.

Results append to a JSON file and finished tasks are skipped on rerun, so a 45 minute eval runs in chunks.

```
python3 tools/eval.py build/jb models/<gguf> build/eval-<name>.json [--limit N] [--only id,id] [--report]
```

## results

3B, this box, 4 threads. Prompt v2 rows use the question-to-source map in the code prompt. One run each, and one run is noisy: the same model and prompt moved a task between pass and fail on reruns. Wall times are suspect, see below.

| model | file | prompt | pass | in the sentence | mean wall | programs per task | compile errors |
|---|---|---|---|---|---|---|---|
| Q6_K | 2.8 GB | v1 | 12/20 | 12 | 160 s | 3.9 | 19 |
| Q6_K | 2.8 GB | v2 | 11/20 | 10 | 195 s | 3.9 | 21 |
| Q8_0 | 3.3 GB | v2 | 13/20 | 12 | 221 s | 3.6 | 15 |
| Q4_0 | 1.9 GB | v2 | 9/9, stopped early | 8 | 324 s | 5.3 | 25 in 9 tasks |

Always right, every run: cpus, diskfree, user, home, kernel, uptime, pagesize, osname, year.
Always wrong, every run: procs, mounts, pidmax, ifaces, cpumodel.
Moves between runs: ram, swap, hostname, etcfiles, passwdsz, passwdln.

## what the numbers say

- **Quant does not move accuracy.** 12, 11, 13 of 20 for Q6_K, Q6_K again, Q8_0. That spread is one run of noise. Q4_0 passed its first 9, then was stopped for time. The tasks that fail, fail on every quant. The ones that pass, pass on every quant. Bits per weight is not where the errors come from.
- **The timing column is confounded.** Another session on this box ran build jobs during part of the eval, and the runs got slower in the order they ran: Q6_K, then Q8_0, then Q4_0. So "Q8_0 is 38% slower" is not a clean measurement. What does hold: M1's per-token rates, where the bigger Q8_0 file is inherently slower to generate from, and the program counts, which do not depend on load. Q4_0 ran 5.3 programs per task against 3.6, because a weaker model makes more mistakes and each mistake is a retry. Q6_K stays on accuracy and retries, not on the wall clock.
- **The prompt map did nothing.** v2's question-to-source map scored one worse. Within noise. The model does not read a map when it has a habit.
- **The footer earns its keep.** In 4 of 69 graded answers the sentence was wrong and the verbatim footer was right. `/etc/passwd occupies 80279486464 bytes` with `1720` underneath.
- **Plan padding is the cost driver.** 3.6 to 5.3 programs per task for questions that need one. Half the wall time answers questions nobody asked. "One is usually enough" was ignored, same as "no commands" in M4.

## failure classes

From the saved logs, the fails sort into three bins.

1. **Wrong probe, right code.** procs, mounts, ifaces answered 4, the CPU count, on every run. cpumodel answered `x86_64`, the machine field of uname, every run. pidmax used a sysconf constant that does not exist, then guessed LONG_MAX, then 255. The program compiles, runs, prints a number, and the number answers a different question. No compiler catches this and the verbatim rule faithfully records it. This is 5 of the 7 stable fails.
2. **Right probe, wrong arithmetic.** passwdln counted 1000000000 lines twice: the program printed a buffer size, not a count. etcfiles reported 784475 directories. The 3B loses track of which variable holds what in a 30-line program.
3. **Ambiguity we caused.** ram vs disk and swap vs ram: the plan asks about all three in one task and the answer step picks the wrong fact for the sentence. Fewer questions would fix this one.

Class 1 is the one worth thinking about. The model has a few probes it trusts, sysconf and statvfs and uname, and maps every question onto one of them. A bigger model has more probes. A retrieval step, "here are the three functions relevant to this question", would probably do more than any prompt wording. A candidate for the next round.

## commands

```
python3 tools/eval.py build/jb models/<gguf> build/eval-<name>.json --limit 3    # a chunk
python3 tools/eval.py build/jb - build/eval-<name>.json --only none --report      # report only
ls build/eval-<name>.json.logs/                                                  # one step log per task
```

## addendum, after the example library (a ten-task subset)

Ten tasks, chosen as five that always passed plus the four that always failed and one action, on the 3B with the current build and library:

```
cpus ram diskfree procs mounts ifaces cpumodel user kernel osname  ->  10/10
```

One program each, 51 to 73 seconds each. The four that had failed on every quant and every prior prompt, procs, mounts, ifaces, cpumodel, all pass, because each now has an exact-shape example.

But two of the always-passing tasks regressed on the first try, and the cause is the library itself. Adding `count_field_value` and three `running` examples diluted the words "processes" and "running": the canonical process question fell to a score of 0.70, and the match bar was a strict greater-than 0.70, so it missed by nothing and the model fell back to its old `sysconf` habit. And `diskfree` had no example of its own, since the original was pruned for its header pile, so it matched the mounts example and tried to read an "avail" column that /proc/mounts does not have.

The fix was two lines and one file: the floor for a two-word match dropped to 0.49, and a clean disk-free example went in. That is the coverage-versus-dilution tension made concrete. Every example added helps its own questions and very slightly hurts every question that shares a common word with it. Rarity weighting absorbs most of that, but the match floor has to leave room, and a query that has no example is exposed to the nearest wrong one. The `--example` switch, which needs no model, is how you catch these: run it on a handful of phrasings after adding examples.


The single-step loop, tried on three of these tasks: same pass and fail as before, one program per task instead of three or four, and 51 to 64 seconds instead of 95 to 215. The full set should be rerun under it when the box is free. Expect the same pass count and about a third of the time.
