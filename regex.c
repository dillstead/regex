// TODO: tmp scratch naming
// gcc -Werror -Wall -Wextra -Wno-error=unused-parameter -Wno-error=unused-function -Wno-error=unused-variable -Wconversion -Wno-error=sign-conversion -fsanitize=undefined -fno-diagnostics-color -DTEST -O0 -g3 -o regex regex.c && echo "no error"
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
typedef uintptr_t  uptr;

#define assert(c)     while (!(c)) *(volatile int *)0 = 0
#define sizeof(x)     (size) sizeof(x)
#define countof(a)    (size)(sizeof(a) / sizeof(*(a)))
#define lengthof(s)   (countof(s) - 1)
#define s8(s)         (struct s8){(u8 *)s, lengthof(s)}
#define s8cstr(s)     (struct s8){(u8 *)s, xstrlen((u8 *)s)}
#define s8nul         (struct s8){(u8 *)"", 1}
#define xset(d, c, n) __builtin_memset(d, c, n)
#define xcpy(d, s, n) __builtin_memcpy(d, s, n)
#define xcmp(d, s, n) __builtin_memcmp(d, s, n)

static usize to_usize(size v)
{
    assert(v >= 0);
    return (usize) v;
}

static u8 to_u8(i64 v)
{
    assert(v >= 0 && v < 256);
    return (u8) v;
}

enum {
    OPEN_RDONLY = 0x0,
    OPEN_WRONLY = 0x1,
    OPEN_RDWR   = 0x2,
};

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

static void *plt_mmap(size, i32, i32);
static size plt_write(i32, u8 *, size);
static size plt_read(i32, u8 *, size);
static _Noreturn void plt_exit(i32 rc);

#include "arena.c"
#include "buf.c"
#include "s8.c"
#include "stack.c"

// The matcher takes (str, length). str must be NULL-terminated and length
// must include the terminator: a match ending on the last character is only
// reported on the NULL-terminator's iteration. 
static b32 (*arch_compile(struct arena, struct s8))(const u8 *, size);

static struct buf in = { (u8[1 << 8]) { 0 }, 0, 1 << 8, 0, 0, 0, 0 };
static struct buf out = { (u8[1 << 8]) { 0 }, 0, 1 << 8, 0, 1, 0, 0 };
static struct buf err = { (u8[1 << 8]) { 0 }, 0, 1 << 8, 0, 2, 0, 0 };

static bool is_literal(u8 c)
{
    return xisalnum(c) || c == ' ';
}
static bool is_valid_char(u8 c)
{
    return is_literal(c) || c == '|' || c == '*' || c == '(' || c == ')';
}

static bool check_syntax(struct s8 re)
{
    if (re.len == 0) {
        return true;
    }

    if (!is_valid_char(re.data[0])
        || re.data[0] == ')'
        || re.data[0] == '*'
        || re.data[0] == '|') {
        return false;
    }

    if (!is_valid_char(re.data[re.len - 1])
        || re.data[re.len - 1] == '('
        || re.data[re.len - 1] == '|') {
        return false;
    }

    for (size i = 0; i < re.len - 1; i++) {
        if (!is_valid_char(re.data[i])) {
            return false;
        }
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

static struct s8 add_concat(struct arena *perm, struct s8 re)
{
    struct s8 cre = { 0 };
    cre.data = new(perm, u8, re.len * 2);

    for (size i = 0; i < re.len - 1; i++) {
        u8 l = re.data[i];
        u8 r = re.data[i + 1];
        cre.data[cre.len++] = l;
        if ((is_literal(l) || l == '*' || l == ')')
             && (is_literal(r) || r == '(')) {
            cre.data[cre.len++] = '.';
        }
    }
    if (re.len) {
        cre.data[cre.len++] = re.data[re.len - 1];
    }
    return cre;
}

static bool to_postfix(struct arena *perm, struct s8 re, struct s8 *pre)
{
    u32 prec[256];
    for (size i = 0; i < countof(prec); i++) {
        prec[i] = 5;
    }
    prec['('] = 1;
    prec['|'] = 2;
    prec['.'] = 3;
    prec['*'] = 4;

    struct arena start = *perm;
    xset(pre, 0, sizeof(*pre));
    pre->data = new(perm, u8, re.len);

    struct arena scratch = *perm;
    struct stack stk;
    stack_init(&scratch, re.len, &stk);

    bool success = true;

    for (size i = 0; success && i < re.len; i++) {
        switch (re.data[i]) {
        case '(': {
            stack_push(&stk, re.data[i]);
            break;
        }
        case ')': {
            bool found = false;
            while (!stack_is_empty(&stk)) {
                i64 val = stack_pop(&stk);
                if (val == '(') {
                    found = true;
                    break;
                }
                pre->data[pre->len++] = to_u8(val);
            }
            if (!found) {
                success = false;
            }
            break;
        }
        default: {
            while (!stack_is_empty(&stk)
                   && (prec[stack_peek(&stk, 0)] >= prec[re.data[i]])) {
                pre->data[pre->len++] = to_u8(stack_pop(&stk));
            }
            stack_push(&stk, re.data[i]);
            break;
        }
        }
    }

    while (success && !stack_is_empty(&stk)) {
        i64 val = stack_pop(&stk);
        if (val != '(') {
            pre->data[pre->len++] = to_u8(val);
        } else {
            success = false;
        }
    }

    if (!success) {
        *perm = start;
    }
    return success;
}

// Rejects a closure whose operand can match the empty string (e.g. a**).
// The code compiled for a** will go into a loop and never return.
static bool check_closures(struct arena scratch, struct s8 pre)
{
    struct stack stk;
    stack_init(&scratch, pre.len, &stk);

    for (size i = 0; i < pre.len; i++) {
        switch (pre.data[i]) {
        case '.': {
            i64 r = stack_pop(&stk);
            i64 l = stack_pop(&stk);
            stack_push(&stk, l && r);
            break;
        }
        case '|': {
            i64 r = stack_pop(&stk);
            i64 l = stack_pop(&stk);
            stack_push(&stk, l || r);
            break;
        }
        case '*': {
            if (stack_pop(&stk)) {
                return false;
            }
            stack_push(&stk, true);
            break;
        }
        default: {
            stack_push(&stk, false);
            break;
        }
        }
    }
    return true;
}

// todo: use scrach arenas
static i32 re_(i32 argc, u8 **argv, struct arena *perm)
{
    if (argc != 2) {
        append_cstr(&err, "usage: regex <regex> < stdin\n");
        return 1;
    }

    struct s8 re = s8cstr(argv[1]);
    if (!check_syntax(re)) {
        append_cstr(&err, "error: syntax\n");
        return 1;
    }

    re = add_concat(perm, re);
    struct s8 pre;
    if (!to_postfix(perm, re, &pre)) {
        append_cstr(&err, "error: mismatched parenthesis\n");
        return 1;
    }

    if (!check_closures(*perm, pre)) {
        append_cstr(&err, "error: empty closure\n");
        return 1;
    }

    b32 (*match)(const u8 *, size) = arch_compile(*perm, pre);
    if (!match) {
        append_cstr(&err, "error: compile\n");
        return 1;
    }

    size cnt;
    for (;;) {
        struct arena scratch = *perm;
        size cap = 1 << 8;
        u8 *line = new(&scratch, u8, cap);
        cnt = get_line(&scratch, &in, &line, &cap);
        if (cnt <= 0) {
            break;
        }

        struct s8 cline = s8cat(&scratch, (struct s8) {line, cnt}, s8nul);
        if (match(cline.data, cline.len)) {
            append(&out, line, cnt);
            if (line[cnt - 1] != '\n') {
                append_cstr(&out, "\n");
            }
        }
    }
    return cnt < 0;
}

#include "test.c"

static i32 test_re_(struct arena *a)
{
    bool passed = test_allowed_chars();
    passed = passed && test_check_syntax();
    passed = passed && test_add_concat(*a);
    passed = passed && test_to_postfix(*a);
    passed = passed && test_check_closures(*a);
    if (passed) {
        append_cstr(&out, "all tests passed\n");
    }
    return passed ? 0 : 1;
}

static void oom_cb(void *usr)
{
    (void) usr;
    append_cstr(&err, "error: out of memory\n");
    flush(&out);
    flush(&err);
}

static i32 re(i32 argc, u8 **argv, u8 *mem, size cap)
{
    struct arena a = { mem, mem + cap, oom_cb, NULL };
#ifndef TEST
    i32 rc = re_(argc, argv, &a);
#else
    i32 rc = test_re_(&a);
#endif
    flush(&out);
    flush(&err);
    return rc;
}

//#if defined(__arm__)
#if 1
#include "arm.c"
#else
#error "Unsupported architecture"
#endif

#if defined(__linux__)
#include <stdlib.h>

#include <sys/mman.h>
#include <unistd.h>

static void *plt_mmap(size sz, i32 prot, i32 flgs)
{
    return mmap(0, to_usize(sz), prot, flgs, -1, 0);
}

static size plt_write(i32 fd, u8 *buf, size len)
{
    return write(fd, buf, to_usize(len));
}

static size plt_read(i32 fd, u8 *buf, size len)
{
    return read(fd, buf, to_usize(len));
}

static _Noreturn void plt_exit(i32 rc)
{
    exit(rc);
}

int main(int argc, char **argv)
{
    size cap = (size) 1 << 24;
    u8 *mem = mmap(0, to_usize(cap), PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANON, -1, 0);
    if (mem == MAP_FAILED) {
        return 1;
    }
    return re(argc, (u8 **) argv, mem, cap);
}

#else
#error "Unsupported platform"
#endif
