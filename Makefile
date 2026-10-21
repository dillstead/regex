# Builds for a 32-bit ARM host (e.g. a Raspberry Pi running armhf).
# Everything is built with -marm.
#
#   make            build everything
#   make check      run the unit tests and replay every seed
#   make clean
#
# The fuzz binaries need AFL++ with its GCC plugin (afl-gcc-fast)
CC      = gcc
FUZZ_CC = afl-gcc-fast

WARN   = -Werror -Wall -Wextra
SAN    = -fsanitize=address,undefined -fno-sanitize-recover=undefined
CFLAGS = -O2 -g

# Raspberry Pi OS preloads libarmmem, so the ASan runtime must be static.
STATIC = -static-libasan

SRCS = regex.c arena.c buf.c s8.c stack.c test.c arm.c

REGEX = regex regex-test
FUZZ  = fuzz fuzz-replay

all: $(REGEX) $(FUZZ)

regex: $(SRCS)
	$(CC) $(WARN) $(CFLAGS) -marm -o $@ regex.c
regex-test: $(SRCS)
	$(CC) $(WARN) $(CFLAGS) $(SAN) $(STATIC) -marm -DTEST -o $@ regex.c

fuzz: fuzz.c $(SRCS)
	$(FUZZ_CC) $(CFLAGS) $(SAN) $(STATIC) -marm -o $@ fuzz.c
fuzz-replay: fuzz.c $(SRCS)
	$(CC) $(CFLAGS) $(SAN) $(STATIC) -marm -o $@ fuzz.c

check: regex-test fuzz-replay
	./regex-test
	for f in afl/seeds/*; do ./fuzz-replay < "$$f" || exit 1; done

clean:
	rm -f $(REGEX) $(FUZZ)

.PHONY: all check clean
