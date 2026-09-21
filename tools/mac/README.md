# ape loader for Apple Silicon

On an M-series Mac an APE file runs through a small loader, `ape`. Apple wants executables signed, and building the loader on the machine produces the signature. Two ways to have it:

**With a compiler.** `xcode-select --install` gives `cc`. Then `./justabuilder` works as is: the APE wrapper builds the loader itself on first run.

**Without a compiler.** Build the loader once, on any Mac that has `cc`, and ship it next to `justabuilder`:

```
cc -O -o ape ape-m1.c            # this file, from the cosmocc toolchain, ISC license
codesign -s - ape                 # ad-hoc signature. codesign is part of macOS, not Xcode
```

On the target Mac:

```
xattr -d com.apple.quarantine ape justabuilder    # once, after download
./ape ./justabuilder --selftest
```

Or install it system wide as `/usr/local/bin/ape`, after which `./justabuilder` alone works.
