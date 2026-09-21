# plan

Goal: one binary. Prompt in. It writes tiny C programs to learn what it needs, runs them, answers. Multiturn.

Model stays on disk for M1 to M5. Embedding comes at M6. Dev loop is faster that way.

## M0 toolchain  [done on linux, see 00-toolchain.md]
Build: cosmocc hello world. Runs on Linux. Copy to a Mac and a Windows box, runs there too.
Learn: what an APE binary is. How one file boots on three OSes.
Done when: `./hello` prints on all three. Linux: yes. Mac, Windows: pending.

## M1 inference  [done, see 01-inference.md]
Build: link llama.cpp static. Load GGUF from a path. Prompt in, text out. Print tok/s.
Learn: GGUF layout. Tokenizer. KV cache. Why quant size drives speed.
Done when: Qwen2.5-Coder-3B answers a C question. Yes. 6 tok/s gen, 14 tok/s prompt on this box. Full quant table moved to M8.

## M2 grammar  [done, see 02-grammar.md]
Build: GBNF grammars for each step in protocol.md. Parser for the output.
Learn: how a grammar masks logits per token. Token boundary problems.
Done when: 100 runs of the plan step, 100 parse clean. Also learned: sample first, check second, or pay 3x.

## M3 runner  [done, see 03-runner.md]
Build: libtcc embedded. Child process compiles and runs a C string. Capture stdout, stderr, exit code. Timeout. Output cap.
Learn: tcc_run vs writing a file. Spawn without fork on Windows. Own header set.
Done when: model-written hello world runs from the binary with no compiler on the host. Yes. Headers are cosmo's own, not ours.

## M4 loop  [done, see 04-loop.md]
Build: state machine PLAN, CODE, COMPILE, FIX, RUN, OBSERVE, ANSWER. Fix loop capped at 3. Facts list.
Learn: why tiny models need small steps. Where they fail.
Done when: "how much disk is free" works end to end. Yes, 30.2 GB. CPUs and RAM too. Compaction by rewind is already in.

## M5 multiturn  [done, see 05-multiturn.md]
Build: REPL. One llama context kept alive. Compaction after each FACT (done in M4). Follow-up questions reuse facts.
Learn: KV reuse. Context budget. What to drop and when.
Done when: 5-turn session stays under 8k tokens. Peak was 593. A follow-up turn costs 4 s and no program.

## M6 embed  [done, see 06-embed.md]
Build: model and header set in the binary. mmap in place. Never written to disk.
Learn: zip store in an APE. Page alignment. How llama.cpp opens a file.
Done when: `ls -la` shows one file and it works on a fresh machine. One 2.8 GB file. 2.49 GB file-backed, 242 MB ours. Fresh machine: pending, needs a box.

## M7 portability  [done, see 07-portability.md]
Build: same binary on Mac and Windows. tcc backends per OS.
Learn: Mach-O JIT rules. PE spawn. Where cosmo leaks.
Done when: M4 demo passes on all three. Yes. Linux x86_64, Linux aarch64 under QEMU, Windows x86_64, macOS arm64 all pass the self-test.

## M8 eval  [done, see 08-eval.md]
Build: 20 read-only IT tasks with known answers. Score pass rate, turns, seconds.
Done when: numbers per quant level. Pick the default quant from the numbers. 12/20 on Q6_K. Q8_0 slower for no gain, Q4_0 slower through retries. Q6_K stays. The errors are meaning errors, not quant errors.

## M9 actions  [done, see 09-actions.md]
DO steps beside NEED. A program performs the action and prints what it did. No gate, `--confirm` opt-in. Whole task as one step first, plan as fallback, NEEDs before DOs. Missing includes repaired without the model. Question tasks got 2 to 3 times faster.

## M10 sandbox (deferred)
`--read-only` exists as a wrapper layer since M9: fopen, open, write, mkdir, unlink, rename, chmod, kill, system, popen, fork, exec, connect, send and syslog are replaced by refusals the model can read. Real enforcement, rlimits and Landlock on Linux, is still this milestone.

## M11 own inference (optional)
Replace llama.cpp with our own GGUF reader and forward pass. Same engine interface. Compare output token for token.

## later

Things noted along the way, not scheduled.

- **Other models.** Switching is one variable, `JB_MODEL`. The prompts were tuned on Qwen2.5-Coder and will want a round of M8 eval on anything else.
- **Thinking models, part done.** A model whose vocab has `<think>` tokens gets an empty think block at the start of every reply, spelled exactly as the model spells it, its own non-thinking convention. Special tokens are banned inside replies and grammars no longer demand a trailing newline. Qwen3-1.7B runs clean at 3 s per step. Still to do: bounded thinking as an experiment, `--think N`: each step's grammar accepts an optional think block of up to N tokens before the format. Measure on the eval with N at 0 and 200 on Qwen3-1.7B. The plan and observe steps are where reasoning could touch the meaning errors, and it costs about 30 s per step at 200 tokens on this box.
- **GPU on Mac.** A cosmo build cannot link Metal. llamafile compiles a small Metal module with Xcode tools on first run and loads it. Big job, big win on Apple hardware. Until then the model runs on the CPU and pins it while generating.
- **Windows on ARM.** cosmo has no native arm64 Windows target. The x86_64 half runs under emulation there and passed the self-test.
- **Windows depth, tiers 1 and 2 done.** PowerShell hinted on Windows hosts, six Win32 wrappers in `<jb_win.h>`. See 11-windows.md. Untested on a Windows box. COM stays out unless a specific interface earns it.
