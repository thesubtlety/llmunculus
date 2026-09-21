# builds third_party/llama.cpp as one static lib with cosmocc
LL   = third_party/llama.cpp
G    = $(LL)/ggml/src
OUT  = build/llama
GEN  = build/gen

CXX    = $(COSMO)/bin/cosmoc++
ARCH   = -Xx86_64-mavx2 -Xx86_64-mfma -Xx86_64-mf16c -Xaarch64-march=armv8.2-a+dotprod+fp16
DEFS   = -DGGML_USE_CPU -DGGML_USE_LLAMAFILE -DGGML_USE_CPU_REPACK -DNDEBUG
INC    = -I$(LL)/include -I$(LL)/ggml/include -I$(G) -I$(G)/ggml-cpu -I$(LL)/src -I$(GEN)/ggml
LCF    = -O2 -mcosmo -fPIC -w $(ARCH) $(DEFS) $(INC)
LCC    = $(LCF) -std=gnu11
LCXX   = $(LCF) -std=gnu++17

GGML_C   = $(G)/ggml.c $(G)/ggml-alloc.c $(G)/ggml-quants.c \
           $(G)/ggml-cpu/ggml-cpu.c $(G)/ggml-cpu/quants.c
GGML_CXX = $(G)/ggml.cpp $(G)/ggml-backend.cpp $(G)/ggml-backend-meta.cpp $(G)/ggml-backend-reg.cpp $(G)/ggml-backend-dl.cpp \
           $(G)/ggml-opt.cpp $(G)/ggml-threading.cpp $(G)/gguf.cpp \
           $(G)/ggml-cpu/ggml-cpu.cpp $(G)/ggml-cpu/repack.cpp $(G)/ggml-cpu/iqp.cpp $(G)/ggml-cpu/hbm.cpp \
           $(G)/ggml-cpu/traits.cpp $(G)/ggml-cpu/amx/amx.cpp $(G)/ggml-cpu/amx/mmq.cpp \
           $(G)/ggml-cpu/binary-ops.cpp $(G)/ggml-cpu/unary-ops.cpp $(G)/ggml-cpu/vec.cpp $(G)/ggml-cpu/ops.cpp \
           $(G)/ggml-cpu/llamafile/sgemm.cpp
ARCH_C   = $(GEN)/ggml-cpu/arch/quants.c
ARCH_CXX = $(GEN)/ggml-cpu/arch/repack.cpp
LLAMA_CXX = $(wildcard $(LL)/src/*.cpp) $(wildcard $(LL)/src/models/*.cpp)

# .c and .cpp with the same stem exist (ggml-cpu.c, ggml-cpu.cpp), so keep the extension in the object name
OBJS = $(patsubst %.c,$(OUT)/%.c.o,$(GGML_C) $(ARCH_C)) \
       $(patsubst %.cpp,$(OUT)/%.cpp.o,$(GGML_CXX) $(ARCH_CXX) $(LLAMA_CXX))

$(OUT)/libllama.a: $(OBJS)
	$(COSMO)/bin/cosmoar rcs $@ $^

$(OUT)/%.c.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(LCC) -c -o $@ $<

$(OUT)/%.cpp.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(LCXX) -c -o $@ $<

# arch wrappers include relative to ggml-cpu/
$(OUT)/$(GEN)/ggml-cpu/arch/%.o: INC += -I$(G)/ggml-cpu
