# SBox Makefile
INCLUDES=-I include -D_GNU_SOURCE -DENABLE_LZ4 -DENABLE_ENCRYPTION
INDENT_FLAGS=-br -ce -i4 -bl -bli0 -bls -c4 -cdw -ci4 -cs -nbfda -l100 -lp -prs -nlp -nut -nbfde -npsl -nss
LIBS=-llz4 -lmbedcrypto

OBJS = \
	release/main.o \
	release/unpack.o \
	release/lz4stream.o \
	release/pack.o \
	release/stream.o \
	release/scan.o \
	release/crc32b.o \
	release/util.o \
	release/io.o \
	release/aes.o

all: host

internal: prepare
	@echo "  CC    src/aes.c"
	@$(CC) $(CFLAGS) $(INCLUDES) src/aes.c -o release/aes.o
	@echo "  CC    src/io.c"
	@$(CC) $(CFLAGS) $(INCLUDES) src/io.c -o release/io.o
	@echo "  CC    src/main.c"
	@$(CC) $(CFLAGS) $(INCLUDES) src/main.c -o release/main.o
	@echo "  CC    src/unpack.c"
	@$(CC) $(CFLAGS) $(INCLUDES) src/unpack.c -o release/unpack.o
	@echo "  CC    src/lz4stream.c"
	@$(CC) $(CFLAGS) $(INCLUDES) src/lz4stream.c -o release/lz4stream.o
	@echo "  CC    src/pack.c"
	@$(CC) $(CFLAGS) $(INCLUDES) src/pack.c -o release/pack.o
	@echo "  CC    src/stream.c"
	@$(CC) $(CFLAGS) $(INCLUDES) src/stream.c -o release/stream.o
	@echo "  CC    src/scan.c"
	@$(CC) $(CFLAGS) $(INCLUDES) src/scan.c -o release/scan.o
	@echo "  CC    src/crc32b.c"
	@$(CC) $(CFLAGS) $(INCLUDES) src/crc32b.c -o release/crc32b.o
	@echo "  CC    src/util.c"
	@$(CC) $(CFLAGS) $(INCLUDES) src/util.c -o release/util.o
	@echo "  LD    release/sbox"
	@$(LD) -o release/sbox $(OBJS) $(LDFLAGS) $(LIBS)

prepare:
	@mkdir -p release

host:
	@make internal \
		CC=gcc \
		LD=gcc \
		CFLAGS='-c -Wall -Wextra -O3 -ffunction-sections -fdata-sections -Wstrict-prototypes' \
		LDFLAGS='-Wl,--gc-sections -Wl,--relax'

install:
	@cp -v release/sbox /usr/bin/sbox

uninstall:
	@rm -fv /usr/bin/sbox
