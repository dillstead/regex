// gcc -Werror -Wall -Wextra -Wno-error=unused-parameter -Wno-error=unused-function -Wno-error=unused-variable -Wconversion -Wno-error=sign-conversion -fsanitize=undefined -fno-diagnostics-color -O0 -g3 -o regex regex.c
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef uint8_t    u8;
typedef int32_t    b32;
typedef int32_t    i32;
typedef uint32_t   u32;
typedef int64_t    i64;
typedef uint64_t   u64;
typedef char       byte;
typedef ptrdiff_t  size;
typedef size_t     usize;
typedef intptr_t   iptr;

struct s8;

#define assert(c)     while (!(c)) *(volatile int *)0 = 0
#define sizeof(x)     (size) sizeof(x)
#define countof(a)    (size)(sizeof(a) / sizeof(*(a)))
#define lengthof(s)   (countof(s) - 1)
#define s8(s)         (struct s8){(u8 *)s, lengthof(s)}
#define s8cstr(s)     (struct s8){(u8 *)s, xstrlen((u8 *)s)}
#define s8nul         (struct s8){(u8 *)"", 1}
#define xset(d, c, n) __builtin_memset(d, c, n)
#define xcpy(d, s, n) __builtin_memcpy(d, s, n)

static usize to_usize(size v)
{
    assert(v >= 0);
    return (usize) v;
}

enum {
    PLT_READ  = 0x1,
    PLT_WRITE = 0x2,
    PLT_EXEC  = 0x4
};

enum {
    PLT_MAP_SHARED  = 0x01,
    PLT_MAP_PRIVATE = 0x02,
    PLT_MAP_ANON    = 0x20
};

#define PLT_MAP_FAILED ((void *) -1)

static u8 *plt_mmap(size, i32, i32);
static b32 plt_write(i32, u8 *, size);
static void plt_exit(i32 rc);

#include "arena.c"
#include "buf.c"
#include "s8.c"

static struct buf out = { (u8[1 << 8]) { 0 }, 1 << 8, 0, 1, 0 };
static struct buf err = { (u8[1 << 8]) { 0 }, 1 << 8, 0, 2, 0 };

static bool ischar(u8 c)
{
    return xisalnum(c) || xisspace(c);
}

static bool check_syntax(struct s8 re)
{
    if (re.len == 1
        && !ischar(re.data[0])) {
        return false;
    }
    
    for (size i = 0; i < re.len - 1; i++) {
        if (re.data[i] == '|') {
            if (re.data[i + 1] == '|'
                || re.data[i + 1] == '*'
                || re.data[i + 1] == ')') {
                return false;
            }
        } else if (re.data[i] == '(') {
            if (re.data[i + 1] == '|'
                || re.data[i + 1] == '*'
                || re.data[i + 1] == ')') {
                return false;
            }
        } else if (re.data[i] == '*') {
            if (re.data[i + 1] == '*') {
                return false;
            }
        }
    }
    return true;
}

static void add_concat(struct arena *a, struct s8 re)
{
    struct s8 cre;
    cre.len = re.len * 2;
    cre.data = new(a, u8, cre.len);

    for (size i = 0; i < re.len; i++) {
    }
}

static bool test_allowed_chars(void)
{
    bool allowed_chars[256] =
    {
        ['0'] = true, ['1'] = true, ['2'] = true, ['3'] = true,
        ['4'] = true, ['5'] = true, ['6'] = true, ['7'] = true,
        ['8'] = true, ['9'] = true,
        ['a'] = true, ['b'] = true, ['c'] = true, ['d'] = true,
        ['e'] = true, ['f'] = true, ['g'] = true, ['h'] = true,
        ['i'] = true, ['j'] = true, ['k'] = true, ['l'] = true,
        ['m'] = true, ['n'] = true, ['o'] = true, ['p'] = true,
        ['q'] = true, ['r'] = true, ['s'] = true, ['t'] = true,
        ['u'] = true, ['v'] = true, ['w'] = true, ['x'] = true,
        ['y'] = true, ['z'] = true,
        ['A'] = true, ['B'] = true, ['C'] = true, ['D'] = true,
        ['E'] = true, ['F'] = true, ['G'] = true, ['H'] = true,
        ['I'] = true, ['J'] = true, ['K'] = true, ['L'] = true,
        ['M'] = true, ['N'] = true, ['O'] = true, ['P'] = true,
        ['Q'] = true, ['R'] = true, ['S'] = true, ['T'] = true,
        ['U'] = true, ['V'] = true, ['W'] = true, ['X'] = true,
        ['Y'] = true, ['Z'] = true,
        ['\f'] = true, ['\n'] = true, ['\r'] = true, ['\t'] = true,
        ['\v'] = true, [' '] = true
    };
    bool passed = true;
    
    for (size i = 0; i < countof(allowed_chars); i++) {
        bool res = ischar((u8) i) == allowed_chars[i];
        passed = passed && res;
        if (!res) {
            append_cstr(&out, "allowed char test ");
            append_size(&out, i);
            append_cstr(&out, " failed\n");
        }
    }
    return passed;
}
    
static bool test_check_syntax(void)
{
    struct test_case {
        struct s8 in;
        bool expected;
    };
    struct test_case tcs[] = {
        { s8(""), true },
        { s8("a"), true },
        { s8("|"), false },
        { s8("*"), false },
        { s8("("), false},
        { s8(")"), false },
        { s8("aa"), true },
        { s8("a|"), true },
        { s8("a*"), true },
        { s8("a("), true },
        { s8("a)"), true },
        { s8("|a"), true },
        { s8("||"), false },
        { s8("|*"), false },
        { s8("|("), true },
        { s8("|)"), false },
        { s8("*a"), true },
        { s8("*|"), true },
        { s8("**"), false },
        { s8("*("), true },
        { s8("*)"), true },
        { s8("(a"), true },
        { s8("(|"), false },
        { s8("(*"), false },
        { s8("(("), true },
        { s8("()"), false },
        { s8(")a"), true },
        { s8(")|"), true },
        { s8(")*"), true },
        { s8(")("), true },
        { s8("))"), true },
    };
    bool passed = true;
    
    for (size i = 0; i <= lengthof(tcs); i++) {
        bool res = check_syntax(tcs[i].in) == tcs[i].expected;
        passed = passed && res;
        if (!res) {
            append_cstr(&out, "check syntax test ");
            append_size(&out, i);
            append_cstr(&out, " failed\n");
        }
    }
    return passed;
}

static i32 test_re_(struct arena *a)
{
    bool passed = test_allowed_chars();
    passed = passed && test_check_syntax();
    if (passed) {
        append_cstr(&out, "all tests passed\n");
    }
    return passed ? 0 : 1;
}

static i32 re_(i32 argc, u8 **argv, struct arena *a)
{
    if (argc < 3) {
        append_cstr(&err, "usage: regex <regex> <string> <string> ...\n");
        return 1;
    }

    struct s8 re = s8cstr(argv[1]);
    if (!check_syntax(re)) {
        append_cstr(&err, "error: syntax\n");
        return 1;
    }
    add_concat(a, re);
    return 0;
}

static i32 re(i32 argc, u8 **argv, u8 *mem, size cap)
{
    struct arena a = { mem, mem + cap, cap };
#ifndef TEST    
    i32 rc = re_(argc, argv, &a);
#else
    i32 rc = test_re_(&a);
#endif    
    flush(&out);
    flush(&err);
    
    return rc;
}

#if defined(__linux__)
#include <stdlib.h>

#include <sys/mman.h>
#include <unistd.h>

static u8 *plt_mmap(size sz, i32 prot, i32 flgs)
{
    return (u8 *) mmap(0, to_usize(sz), prot, flgs, -1, 0);
}

static b32 plt_write(i32 fd, u8 *buf, size len)
{
    return len == write(fd, buf, to_usize(len));
}

static void plt_exit(i32 rc)
{
    _exit(rc);
}

int main(int argc, char **argv)
{
    size cap = (size) 1 << 24;
    u8 *mem = mmap(0, to_usize(cap), PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);

    i32 rc = re(argc, (u8 **) argv, mem, cap);
    _exit(EXIT_SUCCESS);
}

#else
#error "Unsupported platform, consider porting"
#endif
