struct buf
{
    u8 *buf;
    size pos;
    size cap;
    size len;
    int fd;
    int eof;
    int err;
};

static void fill(struct buf *b)
{
    b->len = 0;
    b->pos = 0;
    if (!b->eof && !b->err) {
        size br = plt_read(b->fd, b->buf, b->cap);

        if (br > 0) {
            b->len = br;
        } else {
            b->err = br < 0;
            b->eof = br == 0;
        }
    }
}

static size input(struct buf *b, u8 *dst, size len)
{
    size tot = 0;
    u8 *end = dst + len;
    while (dst < end) {
        if (b->pos == b->len) {
            fill(b);
            if (!b->len) {
                break;
            }
        }

        size left = end - dst;
        size avail = b->len - b->pos;
        size amt = avail < left ? avail : left;

        for (size i = 0; i < amt; i++) {
            dst[i] = b->buf[b->pos + i];
        }
        b->pos += amt;
        dst += amt;
        tot += amt;
    }
    return tot;
}

static void grow_line(struct arena *a, u8 **linep, size *cap)
{
    if (*linep + *cap == a->beg) {
        new(a, u8, *cap);
    } else {
        u8 *line = new(a, u8, *cap * 2);
        xcpy(line, *linep, to_usize(*cap));
        *linep = line;
    }
    *cap *= 2;
}

static size get_line(struct arena *a, struct buf *buf, u8 **linep, size *cap)
{
    size cnt = 0;
    u8 c = 0;

    while (c != '\n' && input(buf, &c, 1) > 0) {
        if (cnt == *cap) {
            grow_line(a, linep, cap);
        }
        (*linep)[cnt++] = c;
    }
    return !buf->err ? cnt : -1;
}

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

#define append_i64(b, i)  append_int((b), (i64) (i))
#define append_size(b, i) append_i64((b), (i))
#define append_cstr(b, s) append((b), (u8 *) (s), lengthof((s)))
#define append_s8(b, s)   append((b), s.data, s.len)
