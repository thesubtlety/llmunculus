# 03 runner

Status: done. Seven test programs, seven correct outcomes. No compiler on the host. Compile plus run is about 100 ms.

## what is it

`run_c(src, timeout, cap)` in `src/runner.c` takes C source as a string and returns stdout, stderr, an exit code or signal, and flags for timeout and compile error. It never runs model code in the agent's own process.

```
parent (runner.c)                     child (tcc_child.c)
  pipe source in  ------------------>  read stdin
  spawn self with --run                tcc: compile in memory
  poll stdout/stderr, cap, timeout  <- run main, exit with its code
  waitpid, report
```

The child is the same executable. `posix_spawn` starts it with `--run`. That works on Linux, Mac and Windows through cosmo. There is no fork in the design, because Windows has none.

## how libtcc runs C in memory

1. `tcc_compile_string` parses and generates machine code into tcc's own sections.
2. `tcc_relocate` lays them out in a malloc'd block and flips the pages to executable with `mprotect`.
3. Undefined symbols are looked up in a table we filled with `tcc_add_symbol`. There is no dynamic linker. If we did not name it, the program cannot call it.
4. `tcc_run` calls `main`. `exit()` inside the program longjmps back out, so the child gets the code and exits with it.
5. A crash is a real signal. It kills the child. The parent sees `signal=11`.

## the pieces we had to supply

**A cross-arch build of libtcc.** One source file, `libtcc.c`, with `-Xx86_64-DTCC_TARGET_X86_64 -Xaarch64-DTCC_TARGET_ARM64`. cosmocc compiles it twice with the right code generator each time. 15 seconds. No patches.

**tcc's runtime objects.** `tcc_run` insists on `runmain.o`, and generated code sometimes needs helpers from `libtcc1.a`. These must be tcc-compiled objects for the target arch. We build two host tcc binaries with gcc, one per target, and use them to compile tcc's `lib/*.c`. Build time only. The results sit in `build/tcc/lib/<arch>/` and the child picks the dir by `#ifdef __x86_64__`.

**Headers.** tcc needs libc headers and cannot use the host's. We use cosmo's own, copied. Two things made that work:

- cosmocc force-includes `libc/integral/normalize.inc` on every compile. We do the same with `-include`.
- cosmo's headers expect gcc's predefined type macros, `__INT32_TYPE__` and about 260 friends. We dump them once from the real compiler into `jb_predefs.h` and force-include that too.

`src/allhdrs.c` lists every header the model may use. `tools/hdrs.py` asks tcc for the closure with `-MD` and copies it. 171 files, under 1 MB. That is what M6 embeds.

**Symbols.** `src/syms.h` lists the libc functions the model may call, about 200. It is the whole API surface, and it is a security boundary as much as a link table.

**Constants.** cosmo keeps errno values and hundreds of other OS constants in variables, not macros, because they differ per OS and cosmo picks at startup. `extern const errno_t ENOENT;`. A tcc program that names `ENOENT` needs its address. `tools/hdrs.py` scans the header closure for these declarations, checks each against `libcosmo.a`, and writes a table. 722 entries.

## the bug that took an hour

Symptom: `tcc_run` failed with `link symbol '__start_' defined twice`, seven times. The native gcc build of the same code worked.

Trail:

1. tcc creates `__start_NAME` for every section whose name is a C identifier. Seven sections had an empty name. Empty passes the check.
2. Instrumented `new_section`. The name argument was `.text`. The copy was empty. `strcpy` had copied nothing.
3. A standalone strcpy test passed. gcc had inlined it as memcpy. Forcing the real call through a volatile function pointer crashed.
4. `nm` showed `strcpy` with type `i`. It is an IFUNC. A resolver runs at startup and picks the AVX or plain version.
5. The runner's symbol table took `&strcpy` to hand to tcc. Taking the address of an IFUNC in a static cosmo binary breaks its resolution, for every caller in the binary. tcc's own `strcpy` call then landed in the resolver, which returns a pointer and copies nothing.

Fix: never take an IFUNC's address. `strcpy` and `strstr` go through one-line wrappers in `src/syms.c`. `make check-syms` compares the table against the binary's IFUNC list and fails if a new one appears.

Lesson: a bug that appears only under one toolchain is usually a fact about that toolchain. Find the fact.

## results

```
--- ok       status=0 signal=0  timeout=0 compile_error=0   cpus=4
--- exitcode status=7
--- compile  status=2           compile_error=1   <string>:2: error: '_SC_NPROCESSORS' undeclared
--- crash    status=-1 signal=11
--- timeout  status=-1 signal=9 timeout=1
--- flood    status=0 out=4096B                    capped, child not blocked
--- errno    status=0                              1 No such file or directory
```

The compile case is the 0.5B's real bug from M1. The error names the symbol. That line is what the fix loop will feed back.

## commands

```
make tcc                 # libtcc for both arches
make build/runtest && ./build/runtest
make check-syms
JB_ROOT=build ./build/runtest --run < prog.c    # run the child by hand
```

## what broke

- Symbol table declared `extern void printf(void)` in a file that also included stdio.h. Moved to its own file with no headers.
- `runmain.o` not found. tcc always links it in run mode. Cross-built it.
- `exit` defined twice. runmain.o provides its own. Dropped ours.
- cosmo's `libc.a` is 8 bytes. The real library is `libcosmo.a`.
- cosmo's `nm` is an APE. Python cannot exec it directly. Run it through `sh`.
- Four constants are declared in headers but defined nowhere. The generator now checks the archive.
- tcc's own `stdarg.h` clashed with cosmo's. Dropped tcc's include dir. Its predefines are compiled in.
