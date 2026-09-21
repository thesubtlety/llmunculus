# 10 report

Status: done. Measurement is the model's job. Delivery is ours.

## why not let the model send it

It can: a DO program may append to a file, call syslog, open a socket, or popen curl. It is a bad idea for delivery, because a small model writes the transport fresh every time. The MiniCPM health check opened a socket and never sent the request. Add JSON escaping, an auth header, a retry, and it gets worse. And libc has no TLS, so an https endpoint from raw sockets is impossible.

So the tool delivers the report itself, in code written once. The model never touches formatting or transport.

## the document

```
{"time":"2026-09-11T22:36:31Z","host":"linux x86_64",
 "task":"is the number of CPUs on this machine less than 10?",
 "answer":"The number of CPUs on this machine is 4, which is less than 10.",
 "facts":["Check the number of CPUs 4"],
 "failed":false,"verdict":"yes"}
```

`facts` are the measured values, verbatim, labeled by the step that produced them. `failed` is true when any step produced nothing. `verdict` is set when the task was a yes/no question. `--json` prints this instead of the sentence.

## destinations

`--report DEST`, sent after every task unless `--report-if-alert`, which sends only when the verdict is yes or a step failed.

| DEST | how |
|---|---|
| `stdout` | print it |
| `file:PATH` | append one line |
| `syslog` | LOG_INFO, ident llmunculus |
| `http://...` | POST as application/json, our own plaintext client |
| `https://...` | POST over TLS with mbedtls, certificate verified against the system CA bundle. no curl. see 15-tls.md |

Delivery runs in the agent process, not the child, so `--read-only` does not affect it. A job that only measures and reports is the read-only case.

## the condition as an exit status

A task whose first word is is, are, does, has, can, should, will, did, was, were gets one more step after the answer: a grammar-bound `yes`, `no` or `unknown` from the facts. The exit status follows: 0 for no or not a yes/no question, 2 for yes, 1 when a step failed. cron and any monitor can act on that without parsing prose.

```
llmunculus --report-if-alert --report https://hooks.example/disk "is the free disk space on / below 10 percent?"
```

sends nothing on a quiet day and a document plus exit 2 when it is not.

## what broke

- The answer field read `...not below 10%.also learned:`. The optional trailing newline from the thinking-model fix meant answers no longer ended with one, and the footer ran into the sentence. The loop now normalizes every answer to end with exactly one newline.
