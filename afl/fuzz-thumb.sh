#!/bin/sh
# Fuzz the -mthumb build (./fuzz-thumb) with the sanitizer environment from
# env.sh.
#
# Usage: afl/fuzz-thumb.sh [afl-fuzz options...]
#   afl/fuzz-thumb.sh             single instance
#   afl/fuzz-thumb.sh -M main     main instance
#   afl/fuzz-thumb.sh -S thumb1   secondary instance
#
# Build first with `make fuzz-thumb`. An existing afl/out is resumed.
set -eu
cd "$(dirname "$0")/.."
. afl/env.sh

bin=./fuzz-thumb
if [ ! -x "$bin" ]; then
    echo "error: $bin not found; run 'make fuzz-thumb' first" >&2
    exit 1
fi

# Resume an existing afl/out instead of refusing to start.
export AFL_AUTORESUME=1

# -i: seed corpus.  -x: dictionary of operator tokens.  -o: findings.
# -t 1000: inputs normally finish in microseconds, so taking more than 1 s
#   counts as a hang, most likely a JIT loop.
exec afl-fuzz -i afl/seeds -x afl/regex.dict -o afl/out -t 1000 "$@" -- "$bin"
