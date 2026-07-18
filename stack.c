struct stack {
    i32 *data;
    size cap;
    size top;
};

static void stack_init(struct arena *a, size cap, struct stack *stk)
{
    xset(stk, 0, sizeof *stk);
    stk->cap = cap;
    stk->data = new(a, i32, cap);
}

static void stack_push(struct stack *stk, i32 val)
{
    assert(stk->top < stk->cap);
    stk->data[stk->top++] = val;
}

static bool stack_is_empty(struct stack *stk)
{
    return !stk->top;
}

static i32 stack_pop(struct stack *stk)
{
    assert(!stack_is_empty(stk));
    return stk->data[--(stk->top)];
}

static i32 stack_peek(struct stack *stk)
{
    assert(!stack_is_empty(stk));
    return stk->data[stk->top - 1];
}
