// Extracts the signed offset encoded in a B or BL instruction
static i32 extract_off(u32 opcode)
{
    assert((opcode & 0xFF000000) == 0xEA000000 ||
           (opcode & 0xFF000000) == 0xEB000000);
    return (i32) (opcode << 8) >> 6;
}

// Returns the target address encoded by a B or BL opcode located at here
static i32 extract_target(u32 opcode, i32 here)
{
    return here + 8 + extract_off(opcode);
}

// Returns the imm24 field of a B or BL at here that branches to target
static u32 encode_off(i32 here, i32 target)
{
    i32 offset = target - (here + 8);
    assert(offset % sizeof(u32) == 0);
    // off range is +-32MB
    assert(offset >= -(1 << 25) && offset < (1 << 25));
    return ((u32) offset >> 2) & 0x00FFFFFF;
}

// Updates a B or BL opcode located at here to target target
static u32 update_off(u32 opcode, i32 here, i32 target)
{
    assert((opcode & 0xFF000000) == 0xEA000000 ||
           (opcode & 0xFF000000) == 0xEB000000);
    return (opcode & 0xFF000000) | encode_off(here, target);
}

// Emits b target opcode
static u32 emit_b(i32 here, i32 target)
{
    return 0xEA000000 | encode_off(here, target);
}

// emits CMP reg, c opcode
static u32 emit_cmp(u8 reg, u8 c)
{
    return 0xE3500000 | ((u32)(reg & 0xF) << 16) | c;
}

// emits bl target opcode
static u32 emit_bl(i32 here, i32 target)
{
    return 0xEB000000 | encode_off(here, target);
}

// Offsets are in bytes.
enum {
    START_CODE0_OFF = 53 * sizeof(u32),
    CNODE_OFF       = 54 * sizeof(u32),
    NNODE_OFF       = 78 * sizeof(u32),
    MAXCCNT_OFF     = 95 * sizeof(u32),
    ALPHA_B_OFF     =  0 * sizeof(u32),
    ALPHA_BL_OFF    =  3 * sizeof(u32),
    ALPHA_CMP_OFF   =  1 * sizeof(u32),
    CLOSURE_BL_OFF  =  0 * sizeof(u32),
    CLOSURE_REP_OFF =  1 * sizeof(u32),
    OR_B_OFF        =  0 * sizeof(u32),
    OR_BL_OFF       =  1 * sizeof(u32),
    OR_ALT1_OFF     =  2 * sizeof(u32),
    OR_ALT2_OFF     =  3 * sizeof(u32)
};

// 0xdeadbeef indicates compile time patch
static const u32 runtime_tmpl[] = {
    // INIT:
    0xe92d5ff0,     // push  {r4-r12, lr}
    0xe3e0c000,     // mvn   r12, #0
    0xe3a0b000,     // mov   r11, #0
    0xe3a04000,     // mov   r4, #0
    0xe58f416c,     // str   r4, NCNT
    0xea000009,     // b     XCHG

    // ADRNLIST:
    0xe59f815c,     // ldr   r8, MAXCCNT
    0xe1a08288,     // lsl   r8, r8, #5
    0xe28f9e16,     // adr   r9, CLIST
    0xe0894008,     // add   r4, r9, r8
    0xe1a0f00e,     // mov   pc, lr

    // GETCHA:
    0xe5d02000,     // ldrb  r2, [r0]
    0xe2800001,     // add   r0, r0, #1
    0xe2411001,     // sub   r1, r1, #1
    0xe28bb001,     // add   r11, r11, #1
    0xe1a0f00e,     // mov   pc, lr

    // XCHG:
    0xebfffff4,     // bl    ADRNLIST
    0xe59f6138,     // ldr   r6, NCNT
    0xe1a05286,     // lsl   r5, r6, #5
    0xe0845005,     // add   r5, r4, r5
    0xe28f7e13,     // adr   r7, CLIST
    0xe3560000,     // cmp   r6, #0
    0x1a000008,     // bne   2f
    0xe37c0001,     // cmn   r12, #1
    0x1a000016,     // bne   4f
    0xea000005,     // b     2f
    // 1:
    0xe5948010,     // ldr   r8, [r4, #16]
    0xe5878010,     // str   r8, [r7, #16]
    0xe5948014,     // ldr   r8, [r4, #20]
    0xe5878014,     // str   r8, [r7, #20]
    0xe2844020,     // add   r4, r4, #32
    0xe2877020,     // add   r7, r7, #32
    // 2:
    0xe1540005,     // cmp   r4, r5
    0x3afffff7,     // blo   1b
    0xe37c0001,     // cmn   r12, #1
    0x1a000004,     // bne   3f
    0xe28f803c,     // adr   r8, START
    0xe5878010,     // str   r8, [r7, #16]
    0xe587b014,     // str   r11, [r7, #20]
    0xe2866001,     // add   r6, r6, #1
    0xe2877020,     // add   r7, r7, #32
    // 3:
    0xe24f806c,     // adr   r8, XCHG
    0xe5878010,     // str   r8, [r7, #16]
    0xe58f60cc,     // str   r6, CLIDX
    0xe3a06000,     // mov   r6, #0
    0xe58f60c8,     // str   r6, NCNT
    0xe3510000,     // cmp   r1, #0
    0xca000002,     // bgt   5f
    // 4:
    0xe1a0000c,     // mov   r0, r12
    0xe8bd5ff0,     // pop   {r4-r12, lr}
    0xe12fff1e,     // bx    lr
    // 5:
    0xebffffd6,     // bl    GETCHA
    0xea00002c,     // b     CLIST

    // START:
    0xdeadbeef,     // b     CODE0

    // CNODE:
    0xe28f40a8,     // adr   r4, CLIST
    0xe59f609c,     // ldr   r6, CLIDX
    0xe2865001,     // add   r5, r6, #1
    0xe1a05285,     // lsl   r5, r5, #5
    0xe0845005,     // add   r5, r4, r5
    // 1:
    0xe5948010,     // ldr   r8, [r4, #16]
    0xe158000e,     // cmp   r8, lr
    0x0a00000d,     // beq   3f
    0xe2844020,     // add   r4, r4, #32
    0xe1540005,     // cmp   r4, r5
    0x3afffff9,     // blo   1b
    // 2:
    0xe5158010,     // ldr   r8, [r5, #-16]
    0xe5858010,     // str   r8, [r5, #16]
    0xe515800c,     // ldr   r8, [r5, #-12]
    0xe5858014,     // str   r8, [r5, #20]
    0xe2455020,     // sub   r5, r5, #32
    0xe1550003,     // cmp   r5, r3
    0x8afffff8,     // bhi   2b
    0xe583e010,     // str   lr, [r3, #16]
    0xe583a014,     // str   r10, [r3, #20]
    0xe2866001,     // add   r6, r6, #1
    0xe58f604c,     // str   r6, CLIDX
    // 3:
    0xe28ee004,     // add   lr, lr, #4
    0xe1a0f00e,     // mov   pc, lr

    // NNODE:
    0xe1a0700e,     // mov   r7, lr
    0xebffffb5,     // bl    ADRNLIST
    0xe59f603c,     // ldr   r6, NCNT
    0xe1a05286,     // lsl   r5, r6, #5
    0xe0845005,     // add   r5, r4, r5
    0xea000003,     // b     2f
    // 1:
    0xe5948010,     // ldr   r8, [r4, #16]
    0xe1580007,     // cmp   r8, r7
    0x0a000006,     // beq   3f
    0xe2844020,     // add   r4, r4, #32
    // 2:
    0xe1540005,     // cmp   r4, r5
    0x3afffff9,     // blo   1b
    0xe5857010,     // str   r7, [r5, #16]
    0xe585a014,     // str   r10, [r5, #20]
    0xe2866001,     // add   r6, r6, #1
    0xe58f6008,     // str   r6, NCNT
    // 3:
    0xe1a0f003,     // mov   pc, r3
    // MAXCCNT:
    0xdeadbeef,     // .word
    // CLIDX:
    0x00000000,     // .word
    // NCNT:
    0x00000000      // .word
};

// CLIST and NLIST entries
static const u32 list_tmpl[] = {
    0xe1a0300f,     // mov   r3, pc
    0xe2833018,     // add   r3, r3, #24
    0xe59fa004,     // ldr   r10, [pc, #4]
    0xe51ff004,     // ldr   pc, [pc, #-4]
    0x00000000,     // .word target, run time patch
    0x00000000,     // .word start, run time patch
    0x00000000,     // .word pad
    0x00000000      // .word pad
};

static const u32 alpha_tmpl[] = {
    0xdeadbeef,     // b     pc + 1
    0xe3520061,     // cmp   r2, #<alpha>
    0x11a0f003,     // movne pc, r3
    0xdeadbeef,     // bl    NNODE
};

static const u32 closure_tmpl[] = {
    0xdeadbeef,     // bl    CNODE
    0xdeadbeef      // CODE[STACK[lc - 1]]
};

static const u32 or_tmpl[] = {
    0xdeadbeef,     // b     pc + 4
    0xdeadbeef,     // bl    CNODE
    0xdeadbeef,     // CODE[STACK[lc - 1]]
    0xdeadbeef      // CODE[STACK[lc - 2]]
};

static const u32 end_tmpl[] = {
    0xe15a000c,     // cmp   r10, r12
    0x31a0c00a,     // movlo r12, r10
    0xe1a0f003      // mov   pc, r3
};

struct cre_sizes {
    size clist_len;
    size nlist_len;
    size code_sz;
    size total_sz;
};

static b32 get_sizes(struct s8 re, struct cre_sizes *s)
{
    xset(s, 0, sizeof(*s));
    s->total_sz = sizeof(runtime_tmpl);
    for (size i = 0; i < re.len; i++) {
        switch (re.data[i]) {
        case '.': {
            break;
        }
        case '*': {
            s->code_sz += sizeof(closure_tmpl);
            s->clist_len += 1;
            break;
        }
        case '|': {
            s->code_sz += sizeof(or_tmpl);
            s->clist_len += 1;
            break;
        }
        default: {
            s->code_sz += sizeof(alpha_tmpl);
            s->clist_len += 1;
            s->nlist_len += 1;
            break;
        }
        }
    }
    s->clist_len++;  // Account for START
    s->clist_len++;  // Account for XCHG terminator
    s->code_sz += sizeof(end_tmpl);
    s->total_sz += s->code_sz;
    s->total_sz += s->clist_len * sizeof(list_tmpl);
    s->total_sz += s->nlist_len * sizeof(list_tmpl);
    // B/BL range is +-32MB
    return s->total_sz < (1 << 25);
}

static i32 (*arch_compile(struct arena scratch, struct s8 re, size *sz))(const u8 *, size)
{
    struct cre_sizes s;
    if (!get_sizes(re, &s)) {
        append_cstr(&err, "error: too large\n");
        return NULL;
    }

    *sz = s.total_sz;
    u8 *cre = plt_mmap(s.total_sz, PLT_READ | PLT_WRITE | PLT_EXEC,
                       PLT_MAP_PRIVATE | PLT_MAP_ANON);
    if (cre == PLT_MAP_FAILED) {
        append_cstr(&err, "error: mmap failed\n");
        return NULL;
    }

    xcpy(cre, runtime_tmpl, sizeof(runtime_tmpl));
    u8 *base = cre + sizeof(runtime_tmpl);
    for (size i = 0; i < s.clist_len; i++) {
        xcpy(base + i * sizeof(list_tmpl), list_tmpl, sizeof(list_tmpl));
    }
    base += s.clist_len * sizeof(list_tmpl);
    for (size i = 0; i < s.nlist_len; i++) {
        xcpy(base + i * sizeof(list_tmpl), list_tmpl, sizeof(list_tmpl));
    }
    base += s.nlist_len * sizeof(list_tmpl);

    // TODO: remove cast on ARM here and below
    *((u32 *) (cre + START_CODE0_OFF)) = emit_b(START_CODE0_OFF,
                                                (i32) (s.total_sz - s.code_sz));
    *((u32 *) (cre + MAXCCNT_OFF)) = (u32) s.clist_len;

    struct stack stk;
    stack_init(&scratch, re.len, &stk);
    // pc is in bytes
    size pc = base - cre;
    for (size i = 0; i < re.len; i++) {
        assert(pc % sizeof(u32) == 0);
        switch (re.data[i]) {
        case '.': {
            stack_pop(&stk);
            break;
        }            
        case '*': {
            xcpy((u8 *) cre + pc, closure_tmpl, sizeof(closure_tmpl));
            // bl    CNODE
            *((u32 *) (cre + pc + CLOSURE_BL_OFF))
                = emit_bl((i32) pc + CLOSURE_BL_OFF, CNODE_OFF);
            // CODE[STACK[lc - 1]]
            u32 b = *((u32 *) (cre + stack_peek(&stk, 0)));
            i32 target = extract_target(b, (i32) stack_peek(&stk, 0));
            *((u32 *) (cre + pc + CLOSURE_REP_OFF))
                = update_off(b, (i32) pc + CLOSURE_REP_OFF, target);
            // CODE[STACK[lc - 1]] = pc
            *((u32 *) (cre + (i32) stack_peek(&stk, 0)))
                = emit_b((i32) stack_peek(&stk, 0), (i32) pc);
            pc += sizeof(closure_tmpl);
            break;
        }
        case '|': {
            xcpy((u8 *) cre + pc, or_tmpl, sizeof(or_tmpl));
            // b     pc + 4
            *((u32 *) (cre + pc + OR_B_OFF))
                = emit_b((i32) pc, (i32) (pc + 4 * sizeof(u32)));
            // bl    CNODE
            *((u32 *) (cre + pc + OR_BL_OFF))
                = emit_bl((i32) pc + OR_BL_OFF, CNODE_OFF);
            // CODE[STACK[lc - 1]]
            u32 b = *((u32 *) (cre + stack_peek(&stk, 0)));
            i32 target = extract_target(b, (i32) stack_peek(&stk, 0));
            *((u32 *) (cre + pc + OR_ALT1_OFF))
                = update_off(b, (i32) pc + OR_ALT1_OFF, target);
            // CODE[STACK[lc - 2]]
            b = *((u32 *) (cre + stack_peek(&stk, 1)));
            target = extract_target(b, (i32) stack_peek(&stk, 1));
            *((u32 *) (cre + pc + OR_ALT2_OFF))
                = update_off(b, (i32) pc + OR_ALT2_OFF, target);
            // CODE[STACK[lc - 2]] = pc + 1
            *((u32 *) (cre + (i32) stack_peek(&stk, 1)))
                = emit_b((i32) stack_peek(&stk, 1), (i32) (pc + 1 * sizeof(u32)));
            // CODE[STACK[lc - 1]] = pc + 4
            *((u32 *) (cre + (i32) stack_peek(&stk, 0)))
                = emit_b((i32) stack_peek(&stk, 0), (i32) (pc + 4 * sizeof(u32)));
            pc += sizeof(or_tmpl);
            stack_pop(&stk);
            break;
        }
        default: {
            // alpha
            xcpy(cre + pc, alpha_tmpl, sizeof(alpha_tmpl));
            // b     pc + 1
            *((u32 *) (cre + pc + ALPHA_B_OFF))
                = emit_b((i32) pc, (i32) (pc + sizeof(u32)));
            // cmp   r2, #<alpha>
            *((u32 *) (cre + pc + ALPHA_CMP_OFF)) = emit_cmp(2, re.data[i]);
            // TODO: bl NNODE
            *((u32 *) (cre + pc + ALPHA_BL_OFF))
                = emit_bl((i32) pc + ALPHA_BL_OFF, NNODE_OFF);
            stack_push(&stk, pc);
            pc += sizeof(alpha_tmpl);
            break;
        }
        }
    }
    xcpy(cre + pc, end_tmpl, sizeof(end_tmpl));
    // Make the generated code visible to instruction fetch.
    __builtin___clear_cache((char *) cre, (char *) cre + s.total_sz);
    // Mapping ensures bit 0 is 0 so ARM state is entered when executing it.
    return (i32 (*)(const u8 *, size)) cre;
}
