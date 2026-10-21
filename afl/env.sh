# Sanitizer environment for running the AFL binary (fuzz).
# Sourced by fuzz-arm.sh. Source it by hand before running
# afl-tmin or afl-fuzz directly:
#
#   . afl/env.sh
#
# Not for fuzz-replay: when debugging you want ASan's own SEGV report and
# symbolized stack traces, which these settings turn off.
#
# Setting any *SAN_OPTIONS makes AFL++ skip its built-in sanitizer defaults,
# so everything AFL needs has to be spelled out here.

# abort_on_error=1: report an error with abort() (SIGABRT) instead of the
#   default _exit(1). AFL only counts a signal as a crash, and afl-fuzz
#   refuses to start without this option.
# halt_on_error=1: stop at the first error. This is the default when the
#   binary is built without -fsanitize-recover=address, but AFL's defaults
#   include it, so it's stated here too.
# symbolize=0: don't run llvm-symbolizer on every crash. It is slow and AFL
#   keeps no output anyway, and afl-fuzz refuses to start without this option.
#   Replay with fuzz-replay to get a symbolized trace.
# handle_segv=0, handle_sigbus=0, handle_sigill=0, handle_sigfpe=0: leave
#   these signals to the kernel. Otherwise ASan catches a fault inside the
#   JIT (bad branch, guard page over-read, 0xdeadbeef patch slot), prints a
#   report and aborts. AFL would then save it as sig:06 rather than sig:11 or
#   sig:04, and the README's triage depends on that distinction.
# detect_stack_use_after_return=1: catch a pointer to a returned function's
#   stack frame, e.g. something left pointing into an arena that a stage
#   passed by value. It costs some speed, and AFL's defaults turn it off.
# max_malloc_fill_size=1G: ASan fills new malloc memory with garbage (0xbe),
#   but by default only the first 4 KiB of each block. Filling all of it
#   makes reads of uninitialized arena memory go wrong instead of seeing
#   zeros.
ASAN_OPTIONS="abort_on_error=1:halt_on_error=1:symbolize=0"
ASAN_OPTIONS="$ASAN_OPTIONS:handle_segv=0:handle_sigbus=0"
ASAN_OPTIONS="$ASAN_OPTIONS:handle_sigill=0:handle_sigfpe=0"
ASAN_OPTIONS="$ASAN_OPTIONS:detect_stack_use_after_return=1"
ASAN_OPTIONS="$ASAN_OPTIONS:max_malloc_fill_size=$((1 << 30))"
export ASAN_OPTIONS

# halt_on_error=1: stop at the first UBSan error. UBSan's default is to
#   print and carry on. -fno-sanitize-recover=undefined already makes the
#   checks fatal, so this is a backstop in case a build drops that flag.
# abort_on_error=1: die with SIGABRT, not exit(1), so AFL sees a crash. This
#   includes assert(), which UBSan reports as "store to null pointer".
# symbolize=0: same reason as for ASan.
UBSAN_OPTIONS="halt_on_error=1:abort_on_error=1:symbolize=0"
export UBSAN_OPTIONS
