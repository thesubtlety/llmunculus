# 07 portability

Status: Linux x86_64 and aarch64, Windows x86_64, and macOS arm64 all pass the self-test on real hardware or QEMU (see results below). The dual tcc memory mode and the win32 wrappers remain the untested branches. The check on any box is one command.

## pre-flight without the boxes

Three things reduce the odds before anyone copies a file:

1. **Run the arm64 half here.** Every APE carries an aarch64 ELF. `qemu-aarch64` runs it on this x86 box with Linux semantics. That is the first execution ever of tcc's arm64 code generator, the icache flush, and llama.cpp's NEON kernels. Covers Linux aarch64 fully and most of the Mac risk, not Apple's page rules. Needs `apt install qemu-user`.
2. **Run the Windows path under Wine.** cosmo's Windows code is real code: `CreateProcess` behind `posix_spawn`, named pipes, poll emulation. Wine runs it. Fidelity is imperfect, but a hang or crash there is a finding. Needs `apt install wine64`.
3. **Two shots per trip.** If the default tcc memory layout fails, the retry is an environment variable, not a rebuild. See below.

Results:

**QEMU, arm64: ALL PASS.** `qemu-aarch64 build/jb.aarch64.elf --selftest models/<0.5B>.gguf`. Every runner case right. Model loaded in 15.7 s and generated at 1.1 tok/s under emulation. The generated text was byte for byte the text the x86 build produces, so llama.cpp's NEON kernels and the AVX2 kernels agree. This was the first execution of tcc's arm64 code generator, of the arm64 icache flush, and of the arm64 half of every object in the binary. One artifact: on the crash case QEMU prints `uncaught target signal 11` and then stalls on a core dump until our timeout fires, so the runner test shows `signal=9 timeout=1`. Real hardware will show signal 11. The self-test's crash line passed as is.

**Wine: inconclusive.** Even `build/hello`, 600 KB, spins in syscalls under Wine 9.0 and never prints. cosmo's Windows startup and this Wine do not agree, so Wine cannot tell us anything about our code. The Windows path needs a real Windows box.

**Windows, real box: ALL PASS.** `justabuilder.exe --selftest` on a Windows machine passed all twelve lines, model included, 8.0 tok/s. The crash line came back as `signal=11` with cosmo's own message. The host line read `windows x86_64`. If that machine is Windows on ARM, Windows ran our x86_64 half under its emulation layer. cosmo has no native Windows arm64 target, so on such a box emulation is the only option with this toolchain and the speed cost is inherent.

**macOS, Apple Silicon: ALL PASS.** `--selftest` on an M-series Mac, `darwin arm64`, 12 cores: every line passed in the default tcc memory layout, so Apple's page rules accept read-execute code pages made with `mprotect`. The dual mode was not needed. Model load 1.9 s, generation 25.8 tok/s on the CPU, four times this box. CPU only: a cosmo build has no Metal, so all four threads pin while the model generates. That is expected and it is the difference from Metal-backed apps. See plan.md, later.

So: Linux x86_64, Linux aarch64, Windows x86_64 and macOS arm64 all run it. The dual tcc memory mode has not been needed anywhere yet.

## tcc memory modes

tcc puts code and data in one malloc'd block and uses `mprotect` to make code pages read-execute. With `CONFIG_RUNMEM_RO=1` nothing is ever writable and executable at once. That is the layout tcc's own macOS port ships, so it should pass on Apple Silicon.

If a host refuses it anyway, `JB_TCCMEM=dual` switches to tcc's other layout at runtime: a zero-length temp file, unlinked at once, mapped twice. Code is written through the read-write view and run through the read-execute view. tcc calls this its SELinux mode and compiles it in with an `#ifdef`. Our one patch to tcc, `patches/tcc-dual-mem.patch`, twelve lines, makes it a runtime choice. Both modes pass all seven runner cases and the full self-test here.

Why not `MAP_JIT`: Apple's JIT write-protect toggle flips every JIT page for the thread at once. With code and data in one block, the program's own data would go read-only while it runs. The dual view is the right shape for tcc.

The self-test prints the mode on its `host` line and, if `tcc hello` fails in the default mode, prints the retry.

## the one command

Copy `build/justabuilder` to the machine. Then:

```
./justabuilder --selftest
```

Expected, in order:

```
PASS  host           <os> <arch>, exe <path>
PASS  zip headers    /zip/include present, self-contained
PASS  tcc hello      status=3 ... out=hi
PASS  tcc libc       status=0 ... out=<cpus> <free MB on />
PASS  tcc errno      status=0 ... out=1
PASS  compile error  status=2 ... err=<string>:1: error: 'nosuch' undeclared
PASS  crash contained status=-1 signal=<n>
PASS  timeout        status=-1 signal=<n> timeout=1
PASS  output cap     status=0
PASS  embedded model offset 14446592 size 2793411008 aligned=yes
PASS  model load     <seconds>
PASS  generate       "<some words>" <n> tok <rate> tok/s
ALL PASS
```

Each line is one piece. A FAIL says which piece and what came back. Send me the whole output.

## per OS, what to do and what might break

### Linux, any distro

Nothing to install. First run drops a 9 KB loader at `~/.ape-1.10`. If the shell says `run-detectors: unable to find an interpreter`, binfmt_misc is trying to run it under Wine. Fix from the cosmocc README:

```
sudo wget -O /usr/bin/ape https://cosmo.zip/pub/cosmos/bin/ape-$(uname -m).elf
sudo chmod +x /usr/bin/ape
sudo sh -c "echo ':APE:M::MZqFpD::/usr/bin/ape:' >/proc/sys/fs/binfmt_misc/register"
```

Likely fine. This is the tested platform. An aarch64 Linux box exercises the arm64 half of every object and tcc's arm64 code generator, which no run so far has touched.

### macOS, Apple Silicon

Needs the `ape` loader once. With `cc` present the APE wrapper builds it on first run. Without a compiler, build it once on any Mac and ship it beside the file. `tools/mac/README.md` has both, plus the quarantine step. There is no way around the loader: Apple requires a signature and `assimilate` cannot emit an arm64 Mach-O.

What might break, in order of likelihood:

1. **tcc's executable memory.** Apple Silicon refuses pages that are writable and executable at once. tcc's default outside Apple builds asked for exactly that. Our build now sets `CONFIG_RUNMEM_RO=1`: code pages go read-execute, data read-write, never both. That is the layout tcc's own macOS port uses. If `tcc hello` fails with `mprotect failed`, the next step is `MAP_JIT` plus `pthread_jit_write_protect_np` around the writes. cosmo exposes both.
2. **Instruction cache.** After writing code, arm64 needs a cache flush. tcc calls `__clear_cache` under `TCC_TARGET_ARM64`, which is compiled into the aarch64 half. If `tcc hello` crashes with an illegal instruction, this is where to look.
3. **Signal numbers differ.** `crash contained` should show signal 11 or 10 on Mac. Either is a pass.
4. **The model.** llamafile runs on Apple Silicon through the same cosmo mmap path, so `model load` should pass. Generation will be faster than here.

### Windows

Rename the file to `justabuilder.exe`. Windows picks the loader by extension. Run it from cmd or PowerShell. No shell or MSYS needed for the program itself.

What might break:

1. **Crash reporting.** cosmo turns an access violation into a SIGSEGV for the child. How the parent sees it decides whether `crash contained` shows `signal=11` or an exit code. A pass with a different number is fine. A hang is not.
2. **poll on pipes.** cosmo emulates it. `timeout` and `output cap` exercise it hardest.
3. **Paths.** `/zip` works on Windows. `GetProgramExecutableName` returns a path cosmo can reopen. `host` will say `windows x86_64`.
4. **Calling convention.** tcc emits SysV code. cosmo's libc is SysV on every OS and translates only at the Win32 boundary. `tcc libc` calling `sysconf` and `statvfs` is the direct test of this.

## what the self-test does not cover

- A real task. Run `./justabuilder "how many CPUs does this machine have"` after ALL PASS.
- Memory split. `-v` prints it on Linux only. On Mac use Activity Monitor: real memory should be well under the file size.
- Speed. The `generate` line is 13 tokens. Enough to see the order of magnitude.

## what broke

- The product self-test said `embedded model: none inside this file`, on some builds and not others. I first blamed a stale copy. Wrong. The zip reader read each entry's extra field into a 1024-byte buffer and bailed silently when it was longer. The model's extra field holds the page-alignment padding, 0 to 4095 bytes depending on where the model lands, which shifts with every rebuild of `jb`. Some builds fit, some did not. Now the buffer is sized from the header and the bail-out is logged. Two lessons. A silent `break` in a parser is a bug waiting for the right input. And "it passed after a rebuild" is not a diagnosis.
- `JB_DEBUG_ZIP=1` prints the walk. Kept.
