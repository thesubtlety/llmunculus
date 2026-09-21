# usage

```
justabuilder [-v] [-t seconds] [-a attempts] [--read-only] [--confirm] [--json] [--report DEST] [--report-if-alert] [model.gguf] ["task"]
justabuilder --selftest [model.gguf]
justabuilder --explore
justabuilder --remote user@host [--explore | "task"]
```

| what | order tried |
|---|---|
| model | `.gguf` on the command line, `$JB_MODEL`, `model.gguf` beside the executable, the one inside it |
| task | command line, `/zip/task.txt` inside the executable, else a REPL |
| system prompt | `/zip/system.txt` inside the executable, else the built-in one with the host's OS and arch |

Flags: `-v` logs every step to stderr. `-a N` code attempts per step, default 3. `--read-only` lets programs read and compute but refuses writes, deletes, commands and network sends, with a note the model sees. `--confirm` shows each action program and asks before running it, and refuses without a terminal. Off by default: cron. `-t N` sets the per-program run limit in seconds, default 5. A program that times out gets one retry at 4x before the model hears about it.

REPL commands: `/facts` `/reset` `/ctx` `/quit`.

Environment: `JB_MODEL` model path. `JB_TCCMEM=dual` two-view tcc memory for hosts that refuse rwx pages. `JB_FACTS_MAX` bytes of facts before a squash. `JB_ROOT` where headers and tcc objects live, default `/zip` when embedded. `JB_DEBUG_ZIP=1` traces the embedded-member lookup.

## builds

```
make build/jb                                   # the agent alone, dev tree, needs a .gguf argument
make build/justabuilder                         # + headers, tcc runtime, the model. one 2.8 GB file
make build/justabuilder JB_MODEL=other.gguf     # a different model inside
make build/justabuilder-thin                    # + headers and tcc runtime, no model. 15 MB
make build/justabuilder TASK="how much disk is free on /"      # fixed purpose: runs it on start
make build/justabuilder SYSTEM="You are ..."                    # custom system prompt
```

## reports and exit status

`--json` prints a document with the task, the answer, the measured facts verbatim, a failed flag and a verdict. `--report DEST` also sends it: `stdout`, `file:PATH`, `syslog`, or an `http(s)://` URL, after every task, or only on alerts with `--report-if-alert`. A yes/no task gets a one-word verdict. Exit status: 0 ok or no, 2 yes, 1 a step failed. See 10-report.md.

## remote

`justabuilder --remote user@host [--explore | "task"]` copies this binary to the host, runs it there, and prints the output. Nothing installed on the remote. Use the thin build for `--explore`. `JB_SSH` and `JB_SCP` set ports and keys. A probe can also reach out with `ssh host 'cmd'` via jb_run for a single remote fact. See 14-remote.md.

## explore

`justabuilder --explore` prints a concise host profile with no model: identity, hardware, disk, what is running, network and connections, user accounts with dates, key config, package count. About two seconds. See 13-explore.md.

## sizes, and outputting a file

Two size domains. What flows through the model is small on purpose; what a program reads or writes itself is not.

Through the model (bounded):
- task input: a REPL line up to 2048 bytes; the prompt buffer is 8192 bytes
- context window: 8192 tokens, about 6 KB of text
- facts: 4096 bytes total, each fact line at most 80 characters
- program output the model sees: the first 40 lines or 1500 bytes of stdout, 20 lines or 1500 of stderr
- program the model writes: up to 800 tokens per step

By a program itself (effectively unbounded):
- it reads any size file (jb_read_file grows) and writes any size file (jb_write_file, fopen)
- it streams over sockets, sends over https (jb_https), runs commands with any output
- --explore captures 64 KB; a bare program via --run is limited only by memory

So to output a file, or any large result: have a DO program write it straight to disk, a socket, or an https endpoint, and print a one-line receipt. Only the receipt becomes a fact. `save_to_file.c` writes the full process list to a file and prints "wrote N bytes"; `copy_file`, `set_config`, `append_report` and `download_file` are the same shape. Never route large data through the model; route the receipt.

## helpers and examples

Programs may `#include <jb.h>`: read and write files, run a command, split lines, key lookup, first number, directory listing. `examples/*.c` are shown to the model one at a time, the closest by question wording. Add your own: a `// q: <question>` first line, under 40 lines, then `make build/justabuilder`. See 12-helpers.md.

## windows

On a Windows host the model is told about `<jb_win.h>`: user and SID, elevation, privileges, services. Everything else goes through PowerShell with `popen`. See 11-windows.md.

## actions

A task can change things. "write hello world to test.txt", "append the load average to report.log", "log the free disk space to syslog". The model writes a program that does it and prints what it did. That line is shown under the answer as `did:`. Programs run with your privileges and may run commands.

## output

One line, or a few. When the model's answer does not contain a measured value verbatim, that value is printed under it:

```
The current user is burn.
also learned:
- What is the user's home directory? /home/burn
```

That footer is deliberate. Small models paraphrase values while writing prose, `fridg` became `fridgid` once. Values measured by a program are shown as measured.
