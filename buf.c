struct buf
{
    u8 *buf;
    size cap;
    size len;
    int fd;
    int err;
};

static void flush(struct buf *b)
{
    b->err |= b->fd < 0;
    if (!b->err && b->len) {
        b->err |= plt_write(b->fd, b->buf, b->len) < b->len;
        b->len = 0;
    }
}

static void append(struct buf *b, u8 *src, size len)
{
    u8 *end = src + len;
    while (!b->err && src < end) {
        size left = end - src;
        size avail = b->cap - b->len;
        size amt = avail < left ? avail : left;

        for (size i = 0; i < amt; i++) {
            b->buf[b->len + i] = src[i];
        }
        b->len += amt;
        src += amt;

        if (amt < left) {
            flush(b);
        }
    }
}

static void append_int(struct buf *buf, i64 x)
{
    u8 tmp[24];
    u8 *end = tmp + sizeof(tmp);
    u8 *beg = end;
    i64 t = x > 0 ? -x : x;
    do
    {
        *--beg = (u8) ('0' - t % 10);
    } while (t /= 10);
    if (x < 0)
    {
        *--beg = '-';
    }
    append(buf, beg, (end - beg));
}

#define append_i64(b, i)  append_int(b, (i64) i)
#define append_size(b, i) append_i64(b, i) 
#define append_str(b, s)  append(b, (u8 *) s, strlen(s))
#define append_cstr(b, s) append(b, (u8 *) s, lengthof(s))
#define append_s8(b, s)   append(b, s.data, s.len)

