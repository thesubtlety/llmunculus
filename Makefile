COSMO ?= $(HOME)/.local/opt/cosmocc
CC     = $(COSMO)/bin/cosmocc
CFLAGS = -O2 -mcosmo -Wall -Wno-format-truncation

build/hello: src/hello.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -rf build/*

ZIP = $(COSMO)/extra/zip
build/zipread: src/zipread.c
	$(CC) $(CFLAGS) -o $@ $<
	echo "hello from inside the binary" > build/asset.txt
	cd build && $(ZIP) -q -0 zipread asset.txt

include mk/llama.mk
llama: build/llama/libllama.a

LLAMA_INC = -Ithird_party/llama.cpp/include -Ithird_party/llama.cpp/ggml/include
build/infer: src/infer.c build/llama/libllama.a
	$(CC) $(CFLAGS) $(LLAMA_INC) -c -o build/infer.o src/infer.c
	$(CXX) -o $@ build/infer.o build/llama/libllama.a -lpthread -lm

CORE = src/engine.c src/grammar.c src/parse.c src/host.c src/embed.c
build/gramtest: src/gramtest.c $(CORE) build/llama/libllama.a
	$(CC) $(CFLAGS) $(LLAMA_INC) -c -o build/gramtest.o src/gramtest.c
	for f in $(CORE); do $(CC) $(CFLAGS) $(LLAMA_INC) -c -o build/$$(basename $$f .c).o $$f || exit 1; done
	$(CXX) -o $@ build/gramtest.o build/engine.o build/grammar.o build/parse.o build/host.o build/embed.o build/llama/libllama.a -lpthread -lm

include mk/tcc.mk
tcc: build/tcc/libtcc.a
.PHONY: tcc-patch


# fail if the model-facing symbol table names any cosmo IFUNC. see src/syms.c.
check-syms: build/runtest
	@$(COSMO)/bin/x86_64-linux-cosmo-nm build/runtest.com.dbg | awk '$$2=="i"{print $$3}' | sort > build/ifuncs.txt
	@grep -oE "X\([a-z_0-9]+\)" src/syms.h | sed 's/X(//; s/)//' | sort > build/symnames.txt
	@if comm -12 build/ifuncs.txt build/symnames.txt | grep .; then echo "IFUNC in syms.h, wrap it"; exit 1; else echo "check-syms ok"; fi

# gcc's predefined type macros. tcc needs them to read cosmo's headers. drop the four tcc already defines.
build/gen/tcc/predefs.h:
	@mkdir -p build/gen/tcc
	echo | $(COSMO)/bin/x86_64-unknown-cosmo-cc -dM -E - | grep -E "define __(U?INT|CHAR16|CHAR32|SIG_ATOMIC|WINT|SIZEOF|BYTE_ORDER|ORDER_|SCHAR|SHRT|LONG|WCHAR|SIZE_MAX|PTRDIFF|INTMAX|UINTMAX|FLT|DBL|LDBL|BIGGEST)" | grep -vE "INT128|__(U?INTPTR|PTRDIFF|INT64)_TYPE__" > $@

# the header set the model may use, and the constants table. host tcc does the dependency scan.
build/gen/tcc/consts.c: src/allhdrs.c src/jb_win.h src/jb.h $(wildcard examples/*.c) $(wildcard profile/*.c) examples/index.txt profile/index.txt tools/hdrs.py build/gen/tcc/predefs.h build/tcc/host/x86_64-tcc
	python3 tools/hdrs.py $(COSMO)/include build/tcc/host/x86_64-tcc build/include $@ $(COSMO)/x86_64-linux-cosmo/lib/libcosmo.a $(COSMO)/bin/x86_64-linux-cosmo-nm

RUNNER = src/runner.c src/tcc_child.c src/syms.c src/readonly.c src/win32.c src/tls.c src/embed.c build/gen/tcc/consts.c
build/runtest: src/runtest.c $(RUNNER) $(wildcard src/*.h) build/tcc/libtcc.a build/mbedtls/libmbedtls.a build/tcc/lib/x86_64/runmain.o
	$(CC) $(CFLAGS) -Ithird_party/tinycc -Ithird_party/mbedtls/include -o $@ src/runtest.c $(RUNNER) build/tcc/libtcc.a build/mbedtls/libmbedtls.a

JB_SRC = src/jb.c src/loop.c src/selftest.c src/report.c $(CORE) $(RUNNER)
build/jb: $(JB_SRC) $(wildcard src/*.h) build/llama/libllama.a build/tcc/libtcc.a build/mbedtls/libmbedtls.a build/gen/tcc/consts.c
	rm -rf build/obj && mkdir -p build/obj
	for f in $(JB_SRC); do $(CC) $(CFLAGS) $(LLAMA_INC) -Ithird_party/tinycc -Ithird_party/mbedtls/include -c -o build/obj/$$(basename $$f .c).o $$f || exit 1; done
	$(CXX) -o $@ build/obj/*.o build/llama/libllama.a build/tcc/libtcc.a build/mbedtls/libmbedtls.a -lpthread -lm

# the product: jb plus headers, tcc runtime and the model, in one file.
#   JB_MODEL=path.gguf   which model to bake in. JB_MODEL=- for none (thin build, needs a model at run time)
#   TASK="..."           bake in a task: the file runs it on start with no arguments
#   SYSTEM="..."         replace the system prompt
JB_MODEL ?= models/qwen2.5-coder-3b-instruct-q6_k.gguf
EMBED_EXTRA = $(if $(TASK),task=build/gen/task.txt) $(if $(SYSTEM),system=build/gen/system.txt)
build/llmunculus: build/jb tools/embed.py build/gen/tcc/consts.c FORCE
	@mkdir -p build/gen
	$(if $(TASK),printf '%s\n' "$(TASK)" > build/gen/task.txt)
	$(if $(SYSTEM),printf '%s\n' "$(SYSTEM)" > build/gen/system.txt)
	cp build/jb $@
	python3 tools/embed.py $@ $(JB_MODEL) $(EMBED_EXTRA)

# same, no model inside. 15 MB. run with a .gguf argument, $$JB_MODEL, or model.gguf beside it.
build/llmunculus-thin: build/jb tools/embed.py build/gen/tcc/consts.c FORCE
	cp build/jb $@
	python3 tools/embed.py $@ - $(EMBED_EXTRA)
FORCE:

include mk/mbedtls.mk
mbedtls: build/mbedtls/libmbedtls.a
