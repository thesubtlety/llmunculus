# docs

Learning notes for llmunculus. One file per topic.

Style: short sentences. Plain words. No filler.
Code comments: one line, only where the code is not obvious.

| file | what |
|---|---|
| usage.md | flags, env, make variables, what the output means |
| plan.md | milestones, in order |
| decisions.md | what we chose and why |
| architecture.md | parts, data flow, interfaces |
| protocol.md | what the model says, and the grammar that forces it |
| context.md | multiturn, KV cache, compaction |
| 00-toolchain.md | M0. cosmocc, APE format, files inside the binary |
| 01-inference.md | M1. llama.cpp under cosmocc, first tokens, speed numbers |
| 02-grammar.md | M2. GBNF, why grammar-first is slow, what a grammar cannot fix |
| 03-runner.md | M3. libtcc in a child, cosmo headers under tcc, the IFUNC bug |
| 04-loop.md | M4. the state machine, incremental turns, what six runs taught |
| 05-multiturn.md | M5. what carries between tasks and why it is text, the informed NONE |
| 06-embed.md | M6. one file. zip inside an APE, the patch that was not needed, memory proof |
| 07-portability.md | M7. the self-test, what to expect per OS, what is likely to break where |
| 08-eval.md | M8. twenty tasks, three quants, where the errors really come from |
| 09-actions.md | M9. DO steps, no gate, one step first, repairs the loop does itself |
| 10-report.md | reports. the model measures, the tool delivers. verdicts as exit status |
| 11-windows.md | two calling conventions, why wrappers, six Win32 helpers, PowerShell for the rest |
| 12-helpers.md | jb.h and the example library. shorter programs, examples over instructions |
| 13-explore.md | jb --explore, the deterministic no-model host profile |
| 14-remote.md | reaching another host: send the binary, or probe over ssh |
| 15-tls.md | native HTTPS via mbedtls, jb_https, cert verification, no curl |
| 16-qa.md | the QA review: findings across security, build and docs, and the fixes |

Each milestone gets a numbered note as we build it: `01-inference.md`, `02-grammar.md`, ...
A note answers: what is it, how does it work, what did we build, what broke.
