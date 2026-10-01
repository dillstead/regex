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
    XCHG_CODE0_OFF  = 34 * sizeof(u32),
    CNODE_OFF       = 36 * sizeof(u32),
    NNODE_OFF       = 52 * sizeof(u32),
    MAXCCNT_OFF     = 68 * sizeof(u32),
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
    0xe92d43f0,     // push  {r4-r9, lr}
    0xea000008,     // b     XCHG

    // ADRNLIST:
    0xe59f8100,     // ldr   r8, MAXCCNT
    0xe1a08208,     // lsl   r8, r8, #4
    0xe28f9f41,     // adr   r9, CLIST
    0xe0896008,     // add   r6, r9, r8
    0xe1a0f00e,     // mov   pc, lr

    // GETCHA:
    0xe5d02000,     // ldrb  r2, [r0]
    0xe2800001,     // add   r0, r0, #1
    0xe2411001,     // sub   r1, r1, #1
    0xe1a0f00e,     // mov   pc, lr

    // XCHG:
    0xe59f40e4,     // ldr   r4, NCNT
    0xe1a05004,     // mov   r5, r4
    0xebfffff3,     // bl    ADRNLIST
    0xe28f70dc,     // adr   r7, CLIST
    0xea000004,     // b     1f
    // 2:
    0xe596800c,     // ldr   r8, [r6, #12]
    0xe587800c,     // str   r8, [r7, #12]
    0xe2866010,     // add   r6, r6, #16
    0xe2877010,     // add   r7, r7, #16
    0xe2444001,     // add   r4, r4, #-1
    // 1:
    0xe3540000,     // cmp   r4, #0
    0xcafffff8,     // bgt   2b
    0xe24f8038,     // adr   r8, XCHG
    0xe587800c,     // str   r8, [r7, #12]
    0xe58f40ac,     // str   r4, NCNT
    0xe58f50a4,     // str   r5, CCNT
    0xe3510000,     // cmp   r1, #0
    0xca000002,     // bgt   3f
    0xe3a00000,     // mov   r0, #0
    0xe8bd43f0,     // pop   {r4-r9, lr}
    0xe12fff1e,     // bx    lr
    // 3:
    0xebffffe5,     // bl    GETCHA
    0xe1a0300f,     // mov   r3, pc
    0xdeadbeef,     // b     CODE0
    0xea000022,     // b     CLIST

    // CNODE:
    0xe28f4084,     // adr   r4, CLIST
    0xe3a05000,     // mov   r5, #0
    0xe59f6074,     // ldr   r6, CCNT
    // 1:
    0xe594800c,     // ldr   r8, [r4, #12]
    0xe158000e,     // cmp   r8, lr
    0x0a000007,     // beq   2f
    0xe2844010,     // add   r4, r4, #16
    0xe2855001,     // add   r5, r5, #1
    0xe1550006,     // cmp   r5, r6
    0x9afffff8,     // bls   1b
    0xe5148004,     // ldr   r8, [r4, #-4]
    0xe504e004,     // str   lr, [r4, #-4]
    0xe584800c,     // str   r8, [r4, #12]
    0xe58f5048,     // str   r5, CCNT
    // 2:
    0xe28ee004,     // add   lr, lr, #4
    0xe1a0f00e,     // mov   pc, lr

    // NNODE:
    0xe1a0700e,     // mov   r7, lr
    0xe59f403c,     // ldr   r4, NCNT
    0xe3a05000,     // mov   r5, #0
    0xebffffc9,     // bl    ADRNLIST
    0xea000004,     // b     1f
    // 2:
    0xe596800c,     // ldr   r8, [r6, #12]
    0xe1580007,     // cmp   r8, r7
    0x0a000006,     // beq   3f
    0xe2866010,     // add   r6, r6, #16
    0xe2855001,     // add   r5, r5, #1
    // 1:
    0xe1550004,     // cmp   r5, r4
    0xbafffff8,     // blt   2b
    0xe586700c,     // str   r7, [r6, #12]
    0xe2844001,     // add   r4, r4, #1
    0xe58f4008,     // str   r4, NCNT
    // 3:
    0xe1a0f003,     // mov   pc, r3
    // MAXCCNT:
    0xdeadbeef,     // .word
    // CCNT:
    0x00000000,     // .word
    // NCNT:
    0x00000000      // .word
};

// CLIST and NLIST entries
static const u32 list_tmpl[] = {
    0xe1a0300f,     // mov   r3, pc
    0xe2833008,     // add   r3, r3, #8
    0xe51ff004,     // ldr   pc, [pc, #-4]
    0xdeadbeef      // .word
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
    0xe3a00001,     // mov   r0, #1
    0xe8bd43f0,     // pop   {r4-r9, lr}
    0xe12fff1e      // bx    lr
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
    s->clist_len++;  // Account for XCHG terminator
    s->code_sz += sizeof(end_tmpl);
    s->total_sz += s->code_sz;
    s->total_sz += s->clist_len * sizeof(list_tmpl);
    s->total_sz += s->nlist_len * sizeof(list_tmpl);
    // B/BL range is +-32MB
    return s->total_sz < (1 << 25);
}

static b32 (*arch_compile(struct arena scratch, struct s8 re))(const u8 *, size)
{
    struct cre_sizes s;
    if (!get_sizes(re, &s)) {
        append_cstr(&err, "error: too large\n");
        return NULL;
    }

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
    *((u32 *) (cre + XCHG_CODE0_OFF)) = emit_b(XCHG_CODE0_OFF,
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
    return (b32 (*)(const u8 *, size)) cre;
}
