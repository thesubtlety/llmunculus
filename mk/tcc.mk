# libtcc for both arches. the -X flags pick the code generator per arch.
T = third_party/tinycc
# CONFIG_RUNMEM_RO=1: code pages rx, data rw, never rwx. Apple Silicon refuses rwx and some Linux policies do too.
TCC_DEFS = -DONE_SOURCE=1 -DCONFIG_RUNMEM_RO=1 -Xx86_64-DTCC_TARGET_X86_64 -Xaarch64-DTCC_TARGET_ARM64
build/tcc/libtcc.a: $(T)/libtcc.c build/gen/tcc/config.h build/gen/tcc/tccdefs_.h
	@mkdir -p build/tcc
	$(CC) -O2 -w $(TCC_DEFS) -I$(T) -Ibuild/gen/tcc -c -o build/tcc/libtcc.o $<
	$(COSMO)/bin/cosmoar rcs $@ build/tcc/libtcc.o
