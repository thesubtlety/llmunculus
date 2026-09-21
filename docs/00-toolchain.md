# 00 toolchain

Status: done on Linux. Mac and Windows: run `build/hello` there and note the result here.

## what is it

`cosmocc` is GCC 14 plus Cosmopolitan libc. It builds one file that runs on Linux, Mac, Windows and the BSDs, on x86_64 and aarch64. That file is called an APE, actually portable executable.

## setup

```
mkdir -p ~/.local/opt/cosmocc && cd ~/.local/opt/cosmocc
curl -sSL -o cosmocc.zip https://cosmo.zip/pub/cosmocc/cosmocc.zip && unzip -q cosmocc.zip && rm cosmocc.zip
mkdir extra && curl -sSL -o extra/zip https://cosmo.zip/pub/cosmos/bin/zip && chmod +x extra/zip
```

No PATH change needed. The Makefile points at it with `COSMO`.

## how one file runs everywhere

Look at the first bytes of `build/hello`:

```
MZqFpD='
...
' <<'justinemina6f'
```

Three readers see three things.

| reader | sees |
|---|---|
| Windows | `MZ` is a DOS/PE header. Loads as a PE |
| a Unix shell | `MZqFpD='...'` is a valid shell assignment. The script that follows extracts an ELF or Mach-O loader and execs it |
| unzip | a zip central directory at the end of the file |

On Linux the shell path costs a few ms. The script drops a small loader at `~/.ape-1.10` on first run. This box has that file now. Registering APE with `binfmt_misc` skips the shell. Optional.

Every source file compiles twice, x86_64 and aarch64. `apelink` joins them. So arch-only code needs a guard:

```c
#ifdef __x86_64__
  __builtin_cpu_supports("avx2")   // does not exist on arm
#endif
```

That is the first thing that broke. It matters later: llama.cpp is full of arch-specific kernels.

## outputs of one build

| file | what |
|---|---|
| hello | the APE. 600 KB. Ship this |
| hello.com.dbg | plain x86_64 ELF with symbols. For gdb |
| hello.aarch64.elf | plain arm ELF |

## files inside the binary

An APE is also a zip. `unzip -l build/hello` shows `.symtab.amd64`, `.symtab.arm64`, `.cosmo`. Add your own with the cosmo `zip` tool. Use `-0`, store, no deflate:

```
extra/zip -0 build/prog asset.txt
```

The program reads it at `/zip/asset.txt`. `fopen`, `open`, `mmap` all work. See `src/zipread.c`.

## what mmap of /zip really does

Tested with a 64 MB member. Result from `/proc/self/maps` and `/proc/self/status`:

```
r--s 0008f000 .../maptest        <- the exe, mapped shared at the member's page
RssAnon: 65616 kB                <- and a full copy in anonymous memory
```

So cosmo maps the file, then copies the member out. Fine for headers. Wrong for a 2.5 GB model. It would double memory.

Fix for M6: do not go through `/zip`. Open our own executable, read the zip central directory to get the member offset, and hand llama.cpp a FILE* seeked to it. It maps the whole file from 0, so alignment turned out not to matter for this path. See 06-embed.md. The rules below are kept anyway:

1. The member must be stored, not deflated. `zip -0` does that.
2. The member data must start on a page boundary. `zip -0` does not do that. Offset of `asset.txt` was 619600, which is 1104 bytes into a page. llamafile solves this with its `zipalign` tool. We will write or borrow one at M6.

File-backed mmap is what we want anyway. The OS pages the model in from the file, shares it across processes, and can drop pages under pressure. Nothing is written to disk. It is in memory in the best sense.

## commands

```
make build/hello && ./build/hello
make build/zipread && ./build/zipread
unzip -l build/zipread
```

## what broke

- `__builtin_cpu_supports` on the aarch64 pass. Guarded it.
- `file` is not installed here. Used `xxd` and `head -c` instead.
- Makefile had `../$(ZIP)` with an absolute `ZIP`. Removed the prefix.
