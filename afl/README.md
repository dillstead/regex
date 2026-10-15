# Fuzzing regex with AFL++

The harness is `fuzz.c` in the repo root. It follows
<https://nullprogram.com/blog/2025/02/05/>.

## What it does

This fuzzes for crashes only. Match results are ignored.

Each AFL test case is one pattern plus some subject text:

```
x(a|b)*y        <- line 1: the pattern (what argv[1] would be)
xababy          <- every other line is a subject line, like stdin
xabzy
```

For each test case the harness:

1. Runs the pattern through the same pipeline as `re_()`: `check_syntax`,
   `add_concat`, `to_postfix`, `check_closures`, `arch_compile`. If an earlier
   stage rejects the pattern, the test case stops there. That's fine: the
   rejection paths are fuzzed too.
2. Splits the subject text into lines the same way `get_line()` does (the
   `'\n'` is kept, and the last line may lack it). Each line gets a NUL
   terminator and goes to the compiled matcher as `match(s, len + 1)`, just
   like `re_()`.
3. Unmaps the JIT buffer, so persistent mode doesn't leak mappings.

AFL finds these problems:

| Problem                                    | How it shows up                  |
|--------------------------------------------|----------------------------------|
| Bad encoding/branch, or JIT corrupts state | SIGSEGV/SIGILL in the JIT -> crash |
| JIT reads past the NUL terminator          | SIGSEGV on the guard page        |
| JIT never terminates                       | AFL "hang" (timeout)             |
| Front-end overflow or UB                   | ASan/UBSan report -> crash       |
| `assert` fires (e.g. stack under/overflow, branch range) | store to null -> crash |

Details from the blog post:

- **Persistent mode** (`__AFL_LOOP`): one process runs thousands of inputs, so
  it's much faster than forking for each input.
- **Exactly sized copies**: the pattern is copied into a `malloc` buffer of
  exactly its length, so ASan catches reads past its end.
- **Guard page**: the JIT code isn't compiled by GCC, so neither ASan nor AFL
  instrumentation sees it. To catch over-reads anyway, each line is copied so
  its NUL is the last byte before a `PROT_NONE` page.
- **Unity build**: `fuzz.c` does `#define main oldmain` and includes
  `regex.c`, so it can call the `static` pipeline functions directly.
- **Isolated runs**: each run gets a fresh arena and resets the global
  `out`/`err` buffers. Running out of arena memory calls `abort()`: patterns
  are capped at 4096 bytes, so an OOM means a bug.

Note: AFL coverage only comes from the C code (front end, `arch_compile` and
the harness). The generated ARM code gives AFL no feedback. Shapes that reach
new `arch_compile` paths (nested `|`, `*`, parentheses) still get explored.

## Raspberry Pi setup

### 1. You need a 32-bit ARM userland

`regex.c` only builds the ARM back end under `__arm__`, which is the 32-bit
ARM target. Check:

```sh
uname -m          # armv7l or armv6l: OK.  aarch64: see below
getconf LONG_BIT  # must print 32
```

On 64-bit Raspberry Pi OS (`aarch64`), you have three options:

- Use the 32-bit (armhf) Raspberry Pi OS image, or
- Run an armhf container: `docker run -it --platform linux/arm/v7 debian:bookworm`, or
- Use a 32-bit chroot (`debootstrap --arch=armhf`).

The Pi 3/4/5 CPU runs 32-bit code natively in each case.

### 2. Install the tools

```sh
sudo apt update
sudo apt install build-essential git tmux afl++
```

`afl-gcc-fast` needs the GCC plugin headers that match your GCC version:

```sh
gcc --version                        # e.g. 12
sudo apt install gcc-12-plugin-dev   # match the major version
which afl-gcc-fast
```

If the distro's `afl++` package is missing or old, build it from source:

```sh
sudo apt install gcc-12-plugin-dev python3-dev
git clone --depth 1 https://github.com/AFLplusplus/AFLplusplus
cd AFLplusplus
make source-only    # add NO_NYX=1 if it complains about nyx
sudo make install
```

`afl-clang-fast` works too if you have clang/LLVM installed. Use it in place
of `afl-gcc-fast` below.

### 3. Configure the system (once per boot)

AFL refuses to start if core dumps go to a helper program:

```sh
sudo afl-system-config
```

That command sets `core_pattern`, the CPU governor and a few other things. At
minimum it does:

```sh
echo core | sudo tee /proc/sys/kernel/core_pattern
```

## Build

From the repo root (`fuzz.c` includes `regex.c` by relative path). Build the
default (Thumb-2) binary and an `-marm` binary, so both sides of ARM/Thumb
interworking get covered:

```sh
afl-gcc-fast -g3 -fsanitize=address,undefined \
    -fno-sanitize-recover=undefined -o fuzz-thumb fuzz.c
afl-gcc-fast -g3 -marm -fsanitize=address,undefined \
    -fno-sanitize-recover=undefined -o fuzz fuzz.c
```

Build a replay binary with plain gcc as well. Without the AFL macros it reads
one test case from stdin, runs it once and exits:

```sh
gcc -g3 -fsanitize=address,undefined -fno-sanitize-recover=undefined \
    -o fuzz-replay fuzz.c
```

Before fuzzing, check that every seed passes (no output, exit 0):

```sh
for f in afl/seeds/*; do ./fuzz-replay < "$f" || echo "FAIL $f"; done
```

## Run

```sh
export ASAN_OPTIONS="abort_on_error=1:halt_on_error=1:symbolize=0"
ASAN_OPTIONS+=":detect_stack_use_after_return=1"
ASAN_OPTIONS+=":max_malloc_fill_size=$((1<<30))"
export UBSAN_OPTIONS="halt_on_error=1:abort_on_error=1:symbolize=0"

afl-fuzz -i afl/seeds -x afl/regex.dict -o afl/out -t 1000 ./fuzz
```

- `-i afl/seeds`: starting corpus, built from the `test_match` cases.
- `-x afl/regex.dict`: operator tokens, so AFL can splice in `|`, `*`, `(`,
  `)` and newlines.
- `-t 1000`: a test case that runs longer than 1 s counts as a hang. Real
  inputs finish in microseconds, so a hang most likely means the JIT is
  looping.

Run it in `tmux` so it survives an SSH disconnect. A Pi has 4 cores, so you
can run one main instance and three secondaries:

```sh
afl-fuzz -i afl/seeds -x afl/regex.dict -o afl/out -t 1000 -M main ./fuzz
afl-fuzz -i afl/seeds -x afl/regex.dict -o afl/out -t 1000 -S thumb1 ./fuzz-thumb
afl-fuzz -i afl/seeds -x afl/regex.dict -o afl/out -t 1000 -S arm2 ./fuzz
afl-fuzz -i afl/seeds -x afl/regex.dict -o afl/out -t 1000 -S thumb3 ./fuzz-thumb
```

To check on all instances: `afl-whatsup afl/out`.

### Reading the status screen

- **saved crashes**: anything above 0 is a finding. See below.
- **saved hangs**: probably a JIT infinite loop (or a front-end loop).
  Replaying a hang with `fuzz-replay` just never returns. Run it under
  `timeout 5` or gdb and interrupt it to see where it's stuck.
- **corpus count / map density**: these grow quickly at first, then level
  off. Once "last new find" has stayed at hours for a while, the fuzzer has
  run out of new paths to explore.
- **exec speed**: persistent mode with ASan on a Pi should manage thousands of
  execs per second. If it shows tens, something is wrong (e.g. persistent mode
  isn't active because `afl-gcc-fast` wasn't used).

Stop with Ctrl-C. To resume, run the same command again with `-i -`.

## Triage

### Where findings go

Findings land in `afl/out/<instance>/crashes/` and `afl/out/<instance>/hangs/`.
`<instance>` is `default` for a single `afl-fuzz` with no `-M`/`-S`; otherwise
it's the name you gave (`main`, `thumb1`, ...). The examples below use
`default`.

Each file is the exact input bytes. The name records how AFL found it:

```
id:000000,sig:06,src:000012,time:83411,execs:220931,op:havoc,rep:4
```

`sig` is a quick first hint:

- `06` (SIGABRT): ASan, UBSan (including `assert`) or `abort()` caught
  something in C code.
- `11` (SIGSEGV) or `04` (SIGILL): often a crash inside the JIT, which has no
  sanitizer coverage. It can also be a read past the end of the line (the
  guard page). Go to "Crashes inside the JIT".

Files in `hangs/` ran longer than `-t`. AFL killed them, so there's no signal.

### Replay with a readable report

```sh
ASAN_OPTIONS=symbolize=1 UBSAN_OPTIONS=symbolize=1:print_stacktrace=1 \
    ./fuzz-replay < afl/out/default/crashes/id:000000*
```

`fuzz-replay` runs the one input and exits. `symbolize=1` turns stack
addresses back into `file.c:line`. UBSan needs `print_stacktrace=1` to print a
trace at all. What to expect:

| Finding                  | Output                                                    |
|--------------------------|-----------------------------------------------------------|
| ASan memory error        | `AddressSanitizer: heap-buffer-overflow ...` + stack trace |
| UBSan, including `assert`| `runtime error: store to null pointer ...` + stack trace  |
| Crash in the JIT         | `AddressSanitizer: SEGV ...` at a `pc` with no symbol      |
| Hang                     | nothing, it never returns                                  |

For a hang, use `timeout 5 ./fuzz-replay < hang`. To see where it's stuck, run
it under gdb (below), press Ctrl-C, and look at `$pc`.

### Look at the input

```sh
od -c afl/out/default/crashes/id:000000*
```

Fuzzer inputs contain NULs and control bytes that `cat` hides. Everything
before the first `\n` is the pattern; the rest are subject lines.

### Shrink it

```sh
afl-tmin -i afl/out/default/crashes/id:000000* -o min.txt -- ./fuzz
```

Export the same `ASAN_OPTIONS`/`UBSAN_OPTIONS` you fuzzed with. Without them a
sanitizer error is a plain `exit(1)`, and `afl-tmin` sees no crash. `afl-tmin`
keeps any crash, not necessarily the same one, so replay `min.txt` to confirm
it's still the same bug. Use the binary that found the crash: `./fuzz`
(`-marm`) or `./fuzz-thumb`.

### Confirm against the real program

Line 1 of the test case is the pattern (`argv[1]`); the rest is stdin. Build
`regex` with the same instruction set as the fuzz binary that found it: add
`-marm` for crashes from `fuzz`, leave it out for crashes from `fuzz-thumb`.

```sh
gcc -g3 -marm -fsanitize=address,undefined -fno-sanitize-recover=undefined \
    -o regex regex.c                       # no -DTEST
tail -n +2 min.txt | ./regex "$(head -n 1 min.txt)"
```

A NUL can't go through `argv`: bash drops it with a warning. So a pattern
containing one can't be reproduced this way. `check_syntax` rejects NUL,
though, so this only matters for a bug that fires before that check runs.

If `regex` doesn't crash but the harness does, the harness is doing something
`re_()` doesn't. Fix the harness.

### Keep it as a regression test

Once the bug is fixed, add the minimized case to the matching table in
`test.c` (`test_match`, `test_check_closures`, ...). The `-DTEST` build then
catches it with no fuzzing. Also copy the file into `afl/seeds/`, so future
runs start from it.

### Crashes inside the JIT

The generated code has no symbols. Find the matcher's base address, then turn
the faulting `pc` into an offset into the buffer:

```sh
gdb --args ./fuzz-replay
(gdb) break arch_compile
(gdb) run < min.txt
(gdb) finish                  # "Value returned ... 0x..." is the base
(gdb) continue                # runs until the SIGSEGV/SIGILL
(gdb) p/x $pc - BASE          # byte offset into the JIT buffer
(gdb) set arm force-mode arm  # the JIT is ARM code; gdb may guess Thumb
(gdb) x/8i $pc - 16
(gdb) info registers          # r0 input ptr, r1 remaining len, r2 char,
                              # r3 resume address, lr return address
(gdb) p $_siginfo._sifields._sigfault.si_addr   # data address, for SIGSEGV
```

Buffer layout (from `CLAUDE.md`):

```
[ runtime_tmpl 284 B ][ CLIST clist_len x 16 ][ NLIST nlist_len x 16 ][ node code ][ end_tmpl 12 B ]
```

- Offset < 284: runtime word `offset / 4`. Words 0-1 INIT, 2-6 ADRNLIST,
  7-10 GETCHA, 11-35 XCHG, 36-51 CNODE, 52-67 NNODE, 68-70 MAXCCNT/CCNT/NCNT.
  Executing 68-70 means a branch went to data.
- In CLIST/NLIST: entry `(offset - 284) / 16`, then word `offset % 16 / 4`.
  A fault there usually means the entry's `.word` jump target is bad.
- In node code: walk the postfix pattern, adding 16 bytes per literal, 8 per
  `*` and 16 per `|` (`.` emits nothing). That finds the node.
- SIGILL on a word that reads `0xdeadbeef`: a patch slot was never filled.

For a SIGSEGV, `si_addr` is the address the instruction tried to access. If
it's in the page just after the line, the matcher read past the NUL
terminator. If it's somewhere unrelated, the matcher used a wild pointer.

## Files

- `fuzz.c`: the harness
- `afl/seeds/`: the starting corpus (one pattern + lines per file)
- `afl/regex.dict`: AFL dictionary of operator tokens
- `afl/out/`: AFL output (gitignored)
