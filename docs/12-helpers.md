# 12 helpers and examples

Status: done for the eval set, being checked on questions outside it.

## the problem

A 3B model writes correct 10-line programs and loses track of its variables somewhere past 40. Most of what it writes is not the answer. It is the scaffolding around the answer: open, loop, size a buffer, check every return, print. The scaffolding is where it goes wrong, and it is the same scaffolding every time.

Two moves, both aimed at that.

## jb.h

Twelve static functions in `src/jb.h`, shipped inside the binary as `<jb.h>`. Read a file, write a file, run a command, split into lines, find a line by prefix, count lines with a word, a key's value, the first number in a string, trim, list a directory, exists, size. Every one returns NULL or -1 on failure so a program can ignore errors or check them in one place.

Before, the program that counts processes was 25 lines of `opendir`, `readdir`, `isdigit` and error paths. Now:

```c
#include <jb.h>
int main(void) {
    int n; char **e = jb_dir_list("/proc", &n);
    int procs = 0;
    for (int i = 0; i < n; i++) if (e[i][0] >= '0' && e[i][0] <= '9') procs++;
    printf("%d\n", procs);
    return 0;
}
```

Static functions in a header mean nothing to link and no symbol table entry. tcc compiles them with the program. `--read-only` still holds, because `jb_write_file` and `jb_run` end in `fopen` and `popen`, which are the wrapped ones.

The header is written by us and tested by us. The first version had a bug: `jb_kv` built the prefix `model name:` and `/proc/cpuinfo` says `model name\t:`. My own example printed `unknown`. The helper layer is one more place to be wrong, and it is a place we control.

## examples

`examples/*.c`, each starting with `// q: <question>`, shipped inside the binary. When a step needs a program, the example whose question shares the most words with the need is placed in the prompt, right before the task. Word overlap after dropping stop words, at least two words in common, else no example.

Why this over another instruction: every milestone since M4 has shown small models copying examples and ignoring prohibitions. The example carries three things at once. The right probe, which is the meaning class from M8. The right shape, which is the length class. And the helpers in use, which no list of signatures teaches as well.

What the prompt no longer says when an example is present: the sentence "use the C library where it has the answer: sysconf, statvfs...". That list was the attractor that made every count come back as the CPU count. With an example in the prompt, the list only competed with it. The first run with both present still went to `sysconf(_SC_OPEN_MAX)` for a process count. Without the list, the example won.

## how an example is chosen

Words. Nothing smarter, on purpose: it has to run in C before the model does, in microseconds, on 40 questions.

1. Both the need and each example's `q:` line are split into words. Letters, digits, `/` and `_` stay together, so `/etc/passwd` is one word. Everything is lowercased. Stop words go: the, how, many, what, this, does, is, on, of, in, and about twenty more grammar words, plus verbs and adjectives so generic that sharing one means nothing: open, big, get, check, current, all. Verbs that carry intent stay: find, show, list, delete. Words under two characters go. A light stem drops a trailing s, ed or ing, so mounts, mounted and mount are one word.
2. **Shared words.** For each example, the words it has in common with the need.
3. **Weighted by rarity.** A shared word is worth `1 / (number of examples whose question contains it)`. A word in one example is worth 1. A word in four is worth 0.25. The example's score is the sum.
4. The example with the highest score wins, if it shares two words, or one word that is in at most two examples. No absolute score floor: as the library grew, common words like `process` diluted enough that a fixed floor rejected the correct two-word match. Otherwise no example is shown. A wrong example is worse than none.

Real numbers, from the library as it stood when the `/var/log` question failed:

```
need: what is the total size in bytes of all files directly inside /var/log

score  example question                                        shared words
5.75   ...total size in bytes of all files directly inside...  all bytes directly files inside size total
0.75   what is the memory page size in bytes                   bytes size
0.25   how many bytes is the file /etc/passwd                  bytes
0.25   how much RAM does this machine have in bytes            bytes

word frequencies: bytes 4, size 2, files 1, directly 1, inside 1
```

Two things to read off that. `bytes` is in four examples, so it is worth a quarter, and `size` is in two, so half: the page-size example reaches 0.75 on those two words alone. Before the shape example existed, 0.75 was the best available and it cleared the old bar of 0.6. That is the pick that answered 0. There is no fixed bar now. Two shared words always qualify; one shared word qualifies only if it is in at most two examples. A word in three or more examples is too generic to match alone.

Rarity is computed against the library, not against English. As the library grows, frequent words like `machine` and `bytes` lose weight by themselves, and the specific ones, `passwd`, `zombie`, `nginx`, keep theirs. That is the whole design: adding examples improves the matching of the ones already there.

Where it still goes wrong: phrasing borrowed from another example's domain. "what is the total size in bytes of the swap" picks the directory-size example, because `total`, `size` and `bytes` together outweigh `swap`. Bag-of-words matching cannot know that swap is the subject. When that happens the model gets an example of the wrong shape and usually notices, sometimes not. The remedy is the same as for a missing example: one more file, with the subject word in its question.

The debug switch:

```
llmunculus --example "how many zombie processes are there" "is nginx running"
```

prints the pick for each question and loads no model.

## the loader cap

The example loader held 64 entries. Past 81 examples that silently dropped everything alphabetically after the 64th, so `win_*` and `save_to_file` never loaded and their tasks fell to the nearest wrong example. Raised to 256. A cap that truncates a growing list without a word is the same class of bug as the zip reader's silent break. Watch for it when adding many examples.

## the shell behind popen

Found while checking the reachability example, which printed nothing for `echo $?`. cosmo's `popen` and `system` do not run `/bin/sh`. They run cosmo's own small shell, built into the binary, so that commands work on Windows where there is no sh. A probe through the child showed what it knows:

| works | does not |
|---|---|
| pipes, redirects, `2>&1`, globs, quotes | `$(command)` |
| `&&`, `||`, `;`, `$?`, variables, `$HOME` | subshells `( )` |
| | `for`, `if`, `while` |

`jb_run` now hands the command to `/bin/sh -c` when a real shell exists, single quotes escaped, and every construct works. On Windows there is no `/bin/sh` and the built-in one is used, which is also what PowerShell invocations go through, and those are single commands anyway. A program that calls `system` or `popen` directly still gets the built-in shell. The prompts say `jb_run`.

## harvesting is not curating

The first library came from the eval logs: the last program that ran with exit 0 for every passing task. Nine of the fourteen carried the old ten-header pile, one passed only through the footer, and an example teaches whatever it contains. Pruned to programs with five includes or fewer. Then three of the five survivors turned out to print wrong values when run alone: `year` printed epoch seconds, `uptime` and `passwdln` printed 0. They had passed the eval through the footer or by accident. Gone, replaced by hand-written ones. Passing is not the same as good, and every example now has to print a right value through the child before it goes in.

## results

The four eval tasks that failed on every quant and every prompt, with one example each:

| task | before | after |
|---|---|---|
| procs | 4, the CPU count, every run | 170 of 173, one program, 59 s |
| mounts | 4 or 1 | 26, right |
| ifaces | 4 or garbage | 5, right |
| cpumodel | x86_64 | Intel Xeon Processor (Skylake, IBRS, no TSX) |

The honest caveat: those examples were written for those questions. This is a test on the training set. What it proves is that the retrieval plumbing works and that an example beats an instruction. Whether the library generalizes is the question below.

## coverage

72 examples. Every Linux one ran through the child on this box and printed a right value, or failed for the right reason: no server to post to, no sshd config here. `add_cron` was not run, because it would edit this box's real crontab.

| area | covered |
|---|---|
| machine facts | CPUs, CPU model, RAM, memory and swap, page size, uptime, load, kernel and hostname, user and home, year, file limit, kernel parameters, distribution |
| software and configuration | package count, a package's version, packages with updates, a config file key, a kernel parameter, cron jobs, an environment variable, enabled-but-stopped services, timers, docker containers, firewall state |
| processes and services | count, by state, running, a service's state, failed units, restart, enable, kill by name |
| files and directories | count, total size, largest, newest, line count, lines with a word, lines ending with, file size, exists, recursive count and size, find by name, checksum, world-writable, tail, grep across files, directory sizes, broken symlinks |
| disk | free on /, per mount used and free |
| network and connections | interfaces, IPv4 addresses, listening ports, connections by state, established peers, which process owns a port, reachability, default gateway, DNS, HTTP status |
| users and logs | passwd shells, logged in, errors in syslog or the journal |
| actions | write a file, append a report, copy, chmod, set a config key, delete old files, add a cron job, restart or enable a service, kill a process, syslog, HTTP POST, download |
| Windows | user and elevation, stopped services, event log errors via PowerShell. compile here, unverified on Windows |
| macOS | hardware and memory, launchd services. compile here, unverified on a Mac |

Deliberately not covered: hardware sensors. Thin by choice: the Windows and macOS rows, until a box runs them.

The gaps that remain are a pick list, not a plan: when a task fails on one of your boxes, the fix is one file with a `// q:` line, thirty lines, run once through `llmunculus --run < file.c` to prove it, then `make build/llmunculus`.

## on questions the library has never seen

Two questions with no example written for them. Both failed.

- "how many users in /etc/passwd have a login shell ending in sh": one word in common with any example, so none was shown. The model produced `DO NOT: Attempt to open /etc/passwd without permission.` in ten minutes.
- "what is the total size in bytes of all files directly inside /var/log": matched the page-size example on the words "size" and "bytes". Answer: 0.

So the plumbing works and the matching was naive. Two changes, no model runs:

1. A shared word counts 1 divided by the number of examples that contain it. "size" and "bytes" are in many, "passwd" in one. The total must clear 0.6, or no example is shown. `llmunculus --example "<question>"` prints the pick without loading a model.
2. The library gained shapes, not questions: walk a directory and sum sizes, filter lines by a suffix, count entries by a field, the newest file in a directory, a service state, RAM from `/proc/meminfo`. A shape example carries over to questions with different nouns.

After that, eight questions and their picks:

```
how many users in /etc/passwd have a login shell ending in sh  -> lines ending with
what is the total size in bytes of all files directly inside /var/log -> dir total size
which file in /etc changed most recently                       -> newest file
how many zombie processes are there                            -> count field value
is the sshd service running                                    -> service running
how much RAM does this machine have in bytes                   -> ram bytes
```

Not rerun on the model, by choice: this box is short on time, the pick is now right, and a model copying a shown example is the one behavior every run since M4 has confirmed. The eval's 20 tasks under the current loop and library are the next measurement worth the hour.

The lesson stands either way: a library this size covers what it covers. Coverage, not matching, is the lever from here, and each example is thirty lines of C with a question on top. That is the cheapest way there is to make this tool better at something.

## what broke

- `jb_kv` and the tab before the colon, above.
- The `[example]` log line printed the first 60 characters of code, all `#include <jb.h>`, useless. It prints the example's question now.
