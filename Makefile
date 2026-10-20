# Builds for a 32-bit ARM host (e.g. a Raspberry Pi running armhf).
# Every target comes in two flavors: -marm (all ARM) and -mthumb (C code in
# Thumb calling the ARM JIT, which exercises interworking).
#
#   make            build everything
#   make check      run both test binaries and replay every seed
#   make clean
#
# The fuzz binaries need AFL++ (see afl/README.md). Override the compilers
# with e.g. `make CC=clang AFL_CC=afl-gcc-fast`.

CC     = gcc
AFL_CC ?= afl-clang-fast

WARN   = -Werror -Wall -Wextra
SAN    = -fsanitize=address,undefined -fno-sanitize-recover=undefined
CFLAGS = -O2 -g

# Raspberry Pi OS preloads libarmmem, so the ASan runtime must be static.
# GCC spells it -static-libasan, clang -static-libsan.
GCC_STATIC   = -static-libasan
CLANG_STATIC = -static-libsan

SRCS = regex.c arena.c buf.c s8.c stack.c test.c arm.c

REGEX = regex regex-thumb regex-test regex-test-thumb
FUZZ  = fuzz fuzz-thumb fuzz-replay fuzz-replay-thumb

all: $(REGEX) $(FUZZ)

regex: $(SRCS)
	$(CC) $(WARN) $(CFLAGS) -marm -o $@ regex.c
regex-thumb: $(SRCS)
	$(CC) $(WARN) $(CFLAGS) -mthumb -o $@ regex.c
regex-test: $(SRCS)
	$(CC) $(WARN) $(CFLAGS) $(SAN) $(GCC_STATIC) -marm -DTEST -o $@ regex.c
regex-test-thumb: $(SRCS)
	$(CC) $(WARN) $(CFLAGS) $(SAN) $(GCC_STATIC) -mthumb -DTEST -o $@ regex.c

fuzz: fuzz.c $(SRCS)
	$(AFL_CC) $(CFLAGS) $(SAN) $(CLANG_STATIC) -marm -o $@ fuzz.c
fuzz-thumb: fuzz.c $(SRCS)
	$(AFL_CC) $(CFLAGS) $(SAN) $(CLANG_STATIC) -mthumb -o $@ fuzz.c
fuzz-replay: fuzz.c $(SRCS)
	$(CC) $(CFLAGS) $(SAN) $(GCC_STATIC) -marm -o $@ fuzz.c
fuzz-replay-thumb: fuzz.c $(SRCS)
	$(CC) $(CFLAGS) $(SAN) $(GCC_STATIC) -mthumb -o $@ fuzz.c

check: regex-test regex-test-thumb fuzz-replay fuzz-replay-thumb
	./regex-test
	./regex-test-thumb
	for f in afl/seeds/*; do ./fuzz-replay < "$$f" || exit 1; done
	for f in afl/seeds/*; do ./fuzz-replay-thumb < "$$f" || exit 1; done

clean:
	rm -f $(REGEX) $(FUZZ)

.PHONY: all check clean
