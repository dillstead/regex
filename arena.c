struct arena {
    u8 *beg;
    u8 *end;
    size sz;
    // Called before out-of-memory.
    void (*cb)(void *usr);
    void *usr;
};

static _Noreturn void oom(struct arena *a)
{
    if (a->cb) {
        a->cb(a->usr);
    }
    plt_exit(1);
}
    
#define new(a, t, n) (t *) alloc(a, sizeof(t), _Alignof(t), n)
static void *alloc(struct arena *a, size sz, size align, size count)
{
    size pad = -(iptr) a->beg & (align - 1);
    size avail = a->end - a->beg - pad;
    if (avail < 0 || count > avail / sz) {
        oom(a);
    }
    
    u8 *p = a->beg + pad;
    a->beg += pad + count * sz;

    return xset(p, 0, (usize) (count * sz));
}
