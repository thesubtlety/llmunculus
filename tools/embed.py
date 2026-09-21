#!/usr/bin/env python3
"""embed.py <ape> <model.gguf|-> : add include/, tcc/lib/ and the model to the APE's zip.
the model is stored uncompressed with its data page aligned, so the program can mmap it in place.
headers and objects are read through /zip, which copies, and that is fine for small files."""
import os, sys, zipfile
PAGE = 4096
ape, model = sys.argv[1], sys.argv[2]
extras = dict(a.split("=", 1) for a in sys.argv[3:])   # task=path system=path
zf = zipfile.ZipFile(ape, "a", compression=zipfile.ZIP_STORED, allowZip64=True)
have = set(zf.namelist())
def add_tree(root, prefix):
    for d, _, files in os.walk(root):
        for f in sorted(files):
            p = os.path.join(d, f); arc = prefix + os.path.relpath(p, root)
            if arc not in have: zf.write(p, arc)
add_tree("build/include", "include/")
add_tree("build/tcc/lib", "tcc/lib/")
add_tree("build/examples", "examples/")
add_tree("build/profile", "profile/")
for member, path in extras.items():
    if member + ".txt" not in have: zf.write(path, member + ".txt")
if model != "-" and "model.gguf" not in have:
    # local header = 30 bytes + name + extra. pad the extra field so the data lands on a page boundary.
    zf.fp.seek(0, 2)
    off = zf.fp.tell()
    name = b"model.gguf"
    size = os.path.getsize(model)
    ZIP64_LIMIT = (1 << 31) - 1                                         # python's threshold, not 4 GB
    big = size > ZIP64_LIMIT or off > ZIP64_LIMIT
    zip64_extra = 20 if big else 0                                      # python prepends a 20-byte zip64 record to our extra
    head = off + 30 + len(name) + zip64_extra
    pad = (-head) % PAGE
    if pad < 4: pad += PAGE
    zi = zipfile.ZipInfo("model.gguf", date_time=(2026, 1, 1, 0, 0, 0))
    zi.compress_type = zipfile.ZIP_STORED
    zi.file_size = size
    zi.extra = b"\x42\x4a" + (pad - 4).to_bytes(2, "little") + b"\0" * (pad - 4)   # id 'JB', length, zeros
    with open(model, "rb") as src:
        with zf.open(zi, "w", force_zip64=big) as dst:
            while True:
                b = src.read(1 << 24)
                if not b: break
                dst.write(b)
zf.close()
zf = zipfile.ZipFile(ape)
for zi in zf.infolist():
    if zi.filename == "model.gguf":
        data = zi.header_offset + 30 + len(zi.filename.encode()) + len(zi.extra)
        # the local header's extra may differ from the central one. read it.
        with open(ape, "rb") as f:
            f.seek(zi.header_offset + 26); n = int.from_bytes(f.read(2), "little"); m = int.from_bytes(f.read(2), "little")
        data = zi.header_offset + 30 + n + m
        print(f"model.gguf at {data} size {zi.file_size} aligned={data % PAGE == 0}")
print(f"{len(zf.namelist())} members, {os.path.getsize(ape)} bytes")
