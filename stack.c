// TODO: stack of sizes?
struct stack {
    i64 *data;
    size cap;
    size top;
};

static void stack_init(struct arena *a, size cap, struct stack *stk)
{
    xset(stk, 0, sizeof *stk);
    stk->cap = cap;
    stk->data = new(a, i64, cap);
}

static size stack_size(struct stack *stk)
{
    return stk->top;
}

static void stack_push(struct stack *stk, i64 val)
{
    assert(stk->top < stk->cap);
    stk->data[stk->top++] = val;
}

static bool stack_is_empty(struct stack *stk)
{
    return !stk->top;
}

static i64 stack_pop(struct stack *stk)
{
    assert(!stack_is_empty(stk));
    return stk->data[--(stk->top)];
}

static i64 stack_peek(struct stack *stk, size bk)
{
    assert(!stack_is_empty(stk));
    assert(bk >= 0 && bk < stack_size(stk));
    
    return stk->data[stk->top - ++bk];
}
