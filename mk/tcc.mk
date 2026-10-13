# libtcc for both arches, plus tcc's own build steps. tinycc's configure writes for one host; we need two cross
# compilers and a fixed config.h, so the steps are spelled out here. HOSTCC builds the small host tools.
T = third_party/tinycc
HOSTCC ?= cc
GT = build/gen/tcc

# the patch makes tcc's two-view memory layout a runtime choice. applied once, checked by a marker in the source.
build/tcc/.patched: patches/tcc-dual-mem.patch
	@mkdir -p build/tcc
	grep -q rt_dual $(T)/tccrun.c || patch -p1 -d $(T) < $<
	touch $@
tcc-patch: build/tcc/.patched

$(GT)/config.h:
	@mkdir -p $(GT)
	printf '#define TCC_VERSION "0.9.28rc"\n#define TCC_GITHASH "%s"\n#define CONFIG_TCC_PREDEFS 1\n#define CONFIG_TCCDIR "/zip/tcc"\n#define CONFIG_TCC_BCHECK 0\n#define CONFIG_TCC_BACKTRACE 0\n#define CONFIG_TCC_SEMLOCK 0\n#define CONFIG_TCC_STATIC 1\n' "$$(git -C $(T) rev-parse --short HEAD 2>/dev/null || echo unknown)" > $@

# tccdefs.h as a C string, made by tcc's own conftest tool
$(GT)/c2str: $(T)/conftest.c
	@mkdir -p $(GT)
	$(HOSTCC) -DC2STR -o $@ $<
$(GT)/tccdefs_.h: $(T)/include/tccdefs.h $(GT)/c2str
	$(GT)/c2str $< $@

# CONFIG_RUNMEM_RO=1: code pages rx, data rw, never rwx. Apple Silicon refuses rwx and some Linux policies do too.
TCC_DEFS = -DONE_SOURCE=1 -DCONFIG_RUNMEM_RO=1 -Xx86_64-DTCC_TARGET_X86_64 -Xaarch64-DTCC_TARGET_ARM64
build/tcc/libtcc.a: $(T)/libtcc.c build/tcc/.patched $(GT)/config.h $(GT)/tccdefs_.h
	$(CC) -O2 -w $(TCC_DEFS) -I$(T) -I$(GT) -c -o build/tcc/libtcc.o $<
	$(COSMO)/bin/cosmoar rcs $@ build/tcc/libtcc.o

# host tcc per target arch: compiles the runtime objects below and scans the header closure for hdrs.py
TCC_TARGET_x86_64 = -DTCC_TARGET_X86_64
TCC_TARGET_arm64  = -DTCC_TARGET_ARM64
build/tcc/host/%-tcc: $(T)/tcc.c build/tcc/.patched $(GT)/config.h $(GT)/tccdefs_.h
	@mkdir -p build/tcc/host
	$(HOSTCC) -O2 -w -DONE_SOURCE=1 $(TCC_TARGET_$*) -I$(T) -I$(GT) -o $@ $< -lm -lpthread

# tcc_run links runmain.o and generated code may need libtcc1.a. both must be tcc output for the target arch.
TCC_RT_COMMON = stdatomic atomic builtin alloca alloca-bt dsohandle
TCC_RT_x86_64 = libtcc1 va_list $(TCC_RT_COMMON)
TCC_RT_arm64  = lib-arm64 armflush $(TCC_RT_COMMON)
build/tcc/lib/%/runmain.o: build/tcc/host/%-tcc $(T)/lib/runmain.c
	@mkdir -p $(@D)
	$< -B$(T) -I$(T) -c $(T)/lib/runmain.c -o $@
build/tcc/lib/%/libtcc1.a: build/tcc/host/%-tcc $(wildcard $(T)/lib/*.c $(T)/lib/*.S)
	@mkdir -p $(@D)
	rm -f $@
	for n in $(TCC_RT_$*); do $< -B$(T) -I$(T) -c $(T)/lib/$$n.[cS] -o $(@D)/$$n.o || exit 1; done
	$< -ar rcs $@ $(foreach n,$(TCC_RT_$*),$(@D)/$(n).o)
TCC_RT = build/tcc/lib/x86_64/libtcc1.a build/tcc/lib/x86_64/runmain.o build/tcc/lib/arm64/libtcc1.a build/tcc/lib/arm64/runmain.o
