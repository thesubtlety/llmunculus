# 06 embed

Status: done. `build/llmunculus` is one 2.8 GB file. Model, headers, tcc runtime, agent. No other files needed.

## what is in the file

```
[ APE executable, 14 MB ]
[ zip: include/** 171 headers, tcc/lib/x86_64/* , tcc/lib/arm64/* , model.gguf 2.79 GB ]
```

`tools/embed.py` adds the members to the APE with Python's zipfile. `make build/llmunculus` copies `build/jb` and runs it. Seven seconds, mostly the copy.

## two ways to read a member

**Small files through `/zip`.** The child asks tcc for `/zip/include/stdio.h` and `/zip/tcc/lib/x86_64/runmain.o`. cosmo opens them like any path. It copies each member into memory on open. For a 4 KB header that is nothing. `src/tcc_child.c` picks `/zip` when `/zip/include/jb_predefs.h` exists, else `build/`.

**The model, mapped from the executable.** M0 showed that `/zip` copies, so 2.8 GB that way would double memory. Instead `src/embed.c` reads our own file as a zip, walks the central directory, and finds where `model.gguf`'s bytes start. Then `engine_open_embedded` opens the executable with `fopen`, seeks to that offset, and hands the FILE pointer to `llama_model_load_from_file_ptr`.

Why that works without any patch:

1. The gguf header reader starts at the FILE pointer's current position and records every offset as an absolute file position.
2. llama's own mmap then maps the whole file from offset 0 and indexes with those absolute offsets.
3. Mapping from 0 needs no alignment. The 14 MB of agent in front of the model is mapped too, and never touched.

Proof, from `/proc/self/status` on a real task with `-v`:

```
[mem] after load  RssAnon:   192336 kB   RssFile:    4516 kB
[mem] at exit     RssAnon:   242304 kB   RssFile: 2487504 kB
```

At exit 242 MB is ours: KV cache, compute buffers, the agent. 2.49 GB is the model, file-backed. The kernel paged it in from the executable, can share it with a second process, and can drop it under pressure. Nothing was copied and nothing was written to disk. That is what "in memory, not on disk" should mean.

## the patch I wrote and deleted

Before reading the gguf code I patched `llama-mmap.cpp` to give `llama_file` a base offset and map at it. Thirteen lines. It built. The model failed to load with `invalid magic characters: 'MZqF'`: the header parse never went through `llama_file` at all. Reading further showed the header reader already handled a mid-file start, with absolute offsets, which my base would have double-counted. The whole fix was one `fseek`. The patch is gone and `third_party/llama.cpp` is clean upstream again.

Lesson: trace the full path a byte takes before changing any of it. The first function you find is rarely the one that matters.

## alignment, kept anyway

`embed.py` still pads the local header so the model's bytes start on a 4 KB boundary. Not needed for the from-zero mapping. Needed if we ever map at the member offset, or use direct IO, and it costs under 4 KB. The tool prints `aligned=True` and the build should keep it that way.

Getting there had one trap. Python switches to zip64 headers when a member passes 2 GB, `(1 << 31) - 1`, not 4 GB. The zip64 record adds 20 bytes to the local header. First build printed `aligned=False`.

## reading a zip from inside

`embed_find` in `src/embed.c`, 80 lines, no library:

1. Read the last 66 KB. Scan back for the end-of-central-directory signature, `PK\5\6`.
2. If its counts are saturated at `0xFFFF` or `0xFFFFFFFF`, the zip64 locator sits 20 bytes before it and points at the zip64 record with the real values. Ours is zip64, because of the model.
3. Walk the central directory. Each entry: 46-byte header, name, extra, comment. Match the name.
4. Sizes and the local header offset may be saturated too. Then they live in the entry's extra field under id 1.
5. Read the local header at that offset. Its own name and extra lengths give the data start. They can differ from the central copy, and do, because of the padding.

cosmo has this code inside its `/zip` support. Writing it again was the point.

## what broke

- `multiple definition of llama_mmap::SUPPORTED`. The aarch64 twin archive `build/llama/.aarch64/libllama.a` still held a member from before M1's object rename. `ar rcs` never removes members. Deleted both archives, re-archived from the objects.
- `aligned=False`. Python's 2 GB zip64 threshold, above.
- The patch, above.

## commands

```
make build/llmunculus                   # cp build/jb, embed headers, tcc runtime, model
./build/llmunculus "how many CPUs"      # embedded model
./build/llmunculus -v models/x.gguf ""  # a .gguf argument still overrides, for dev
unzip -l build/llmunculus | tail -3
./build/llmunculus --run < prog.c       # the child alone, reading /zip
```
