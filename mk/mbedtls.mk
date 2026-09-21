# mbedtls for both arches, one static lib. default config (TLS client, x509, net_sockets over cosmo sockets).
M    = third_party/mbedtls
MOUT = build/mbedtls
MBED_SRC = $(wildcard $(M)/library/*.c)
MBED_OBJ = $(patsubst %.c,$(MOUT)/%.o,$(MBED_SRC))
MBED_CF  = -O2 -mcosmo -w -I$(M)/include -I$(M)/library

$(MOUT)/libmbedtls.a: $(MBED_OBJ)
	$(COSMO)/bin/cosmoar rcs $@ $(MBED_OBJ)
$(MOUT)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(MBED_CF) -c -o $@ $<
