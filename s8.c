struct s8
{
    u8 *data;
    size len;
};

static struct s8 s8cpy(struct arena *a, struct s8 s)
{
    struct s8 r = s;
    r.data = new(a, u8, s.len);
    if (r.len)
    {
        xcpy(r.data, s.data, (usize) r.len);
    }
    return r;
}

static struct s8 s8cat(struct arena *a, struct s8 head, struct s8 tail)
{
    if (!head.data || head.data + head.len != a->beg)
    {
        head = s8cpy(a, head);
    }
    head.len += s8cpy(a, tail).len;
    return head;
}

static size xstrlen(const u8 *s)
{
    size len = 0;
    while (*s++) len++;
    return len;
}

static bool xisalnum(u8 c)
{
    return (c >= '0' && c <= '9')
        || (c >= 'a' && c <= 'z')
        || (c >= 'A' && c <= 'Z');
}

static bool xisspace(u8 c)
{
    return c == '\f' || c == '\n'
        || c == '\r' || c == '\t'
        || c == '\v' || c == ' ';
}
