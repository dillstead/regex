// AFL++ crash fuzz test for regex, after
// https://nullprogram.com/blog/2025/02/05/
//
// Input format: the first line is the pattern, every following line is
// subject text. The pattern goes through the same pipeline as re_() and each
// line is run through the compiled matcher. Results are ignored: only
// crashes, sanitizer reports and hangs count.
//
// Build and run on 32-bit ARM, with the Makefile in the repo root. Everything
// is built with -marm. The ASan runtime is linked statically because
// Raspberry Pi OS preloads libarmmem via /etc/ld.so.preload, and a shared
// ASan runtime refuses to start unless it is loaded first.
//
// AFL builds (afl-gcc-fast, AFL++'s GCC plugin, persistent mode):
//   make fuzz
//     afl-gcc-fast -O2 -g -fsanitize=address,undefined
//         -fno-sanitize-recover=undefined -static-libasan -marm -o fuzz fuzz.c
//   afl/fuzz-arm.sh  (sanitizer environment: afl/env.sh)
//
// Replay builds (plain gcc, no AFL macros) run one input from stdin:
//   make fuzz-replay
//     gcc -O2 -g -fsanitize=address,undefined
//         -fno-sanitize-recover=undefined -static-libasan -marm
//         -o fuzz-replay fuzz.c
//   ./fuzz-replay < afl/out/default/crashes/id:000000,...
//
// The JIT code is not instrumented, so ASan cannot see its memory accesses.
// Each subject line is placed against a PROT_NONE guard page instead, so an
// over-read faults.
#define main oldmain
#include "regex.c"
#undef main

#include <unistd.h>

#ifndef __AFL_FUZZ_TESTCASE_LEN
// Standalone replay: read one test case from stdin and run it once.
static u8 fuzz_stdin[1 << 20];
static size fuzz_stdin_len;
static size fuzz_read_stdin(void)
{
    size len = 0;
    for (;;) {
        size r = plt_read(0, fuzz_stdin + len, countof(fuzz_stdin) - len);
        if (r <= 0) {
            return len;
        }
        len += r;
    }
}
static bool fuzz_once(void)
{
    static bool done;
    bool r = !done;
    done = true;
    return r;
}
#  define __AFL_FUZZ_INIT()
#  define __AFL_INIT()            fuzz_stdin_len = fuzz_read_stdin()
#  define __AFL_FUZZ_TESTCASE_BUF fuzz_stdin
#  define __AFL_FUZZ_TESTCASE_LEN fuzz_stdin_len
#  define __AFL_LOOP(n)           fuzz_once()
#endif

// Longer patterns are not more interesting and only slow the fuzzer down.
enum { FUZZ_MAX_PATTERN = 1 << 12 };

// Subject lines are copied so that the NUL terminator is the last byte before
// a PROT_NONE page.
struct guarded {
    u8 *beg;
    size cap;
};

static struct guarded guarded_init(size cap)
{
    size page = sysconf(_SC_PAGESIZE);
    cap = (cap + page - 1) & -page;
    u8 *p = mmap(0, to_usize(cap + page), PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANON, -1, 0);
    assert(p != MAP_FAILED);
    assert(!mprotect(p + cap, to_usize(page), PROT_NONE));
    return (struct guarded) { p, cap };
}

static u8 *guarded_copy(struct guarded g, const u8 *src, size len)
{
    assert(len + 1 <= g.cap);
    u8 *dst = g.beg + g.cap - (len + 1);
    if (len) {
        xcpy(dst, src, to_usize(len));
    }
    dst[len] = 0;
    return dst;
}

static void fuzz_oom(void *usr)
{
    // The pattern is bounded, so running out of arena is a bug.
    (void) usr;
    abort();
}

static void fuzz_one(struct arena a, struct guarded g, u8 *buf, size len)
{
    size plen = 0;
    while (plen < len && buf[plen] != '\n') {
        plen++;
    }
    if (plen > FUZZ_MAX_PATTERN) {
        return;
    }

    // Exactly sized copy so ASan sees reads past the end of the pattern.
    struct s8 re = { malloc(to_usize(plen ? plen : 1)), plen };
    if (plen) {
        xcpy(re.data, buf, to_usize(plen));
    }

    struct s8 pre;
    if (check_syntax(re)
        && to_postfix(&a, add_concat(&a, re), &pre)
        && check_closures(a, pre)) {
        size match_sz;
        b32 (*match)(const u8 *, size) = arch_compile(a, pre, &match_sz);
        if (match) {
            // Lines as get_line() returns them: '\n' included, the last one
            // may lack it.
            size i = plen + (plen < len);
            while (i < len) {
                size j = i;
                while (j < len && buf[j++] != '\n') {}
                match(guarded_copy(g, buf + i, j - i), j - i + 1);
                i = j;
            }
            plt_munmap((void *) match, match_sz);
        }
    }
    free(re.data);
}

__AFL_FUZZ_INIT();

int main(void)
{
    __AFL_INIT();
    size cap = (size) 1 << 22;
    u8 *mem = malloc(to_usize(cap));
    struct guarded g = guarded_init((size) 1 << 20);
    u8 *buf = __AFL_FUZZ_TESTCASE_BUF;
    while (__AFL_LOOP(10000)) {
        size len = __AFL_FUZZ_TESTCASE_LEN;
        // arch_compile reports to err; don't let it accumulate or flush.
        out.len = err.len = 0;
        out.err = err.err = 0;
        struct arena a = { mem, mem + cap, fuzz_oom, NULL };
        fuzz_one(a, g, buf, len);
    }
    free(mem);  // keep LeakSanitizer quiet at exit
    return 0;
}
