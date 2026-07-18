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
            append_size(&out, i + 1);
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
            append_size(&out, i + 1);
            append_cstr(&out, " failed\n");
        }
    }
    return passed;
}

static bool test_add_concat(struct arena *a)
{
    struct test_case {
        struct s8 in;
        struct s8 expected;
    };
    struct test_case tcs[] = {
        { s8(""),    s8("")      },
        { s8("a"),   s8("a")     },
        { s8("|"),   s8("|")     },
        { s8("*"),   s8("*")     },
        { s8("("),   s8("(")     },
        { s8(")"),   s8(")")     },
        { s8("aa"),  s8("a.a")   },
        { s8("aaa"), s8("a.a.a") },
        { s8("a|"),  s8("a|")    },
        { s8("a*"),  s8("a*")    },
        { s8("a("),  s8("a.(")   },
        { s8("a)"),  s8("a)")    },
        { s8("|a"),  s8("|a")    },
        { s8("|("),  s8("|(")    },
        { s8("*a"),  s8("*.a")   },
        { s8("*|"),  s8("*|")    },
        { s8("*("),  s8("*.(")   },
        { s8("*)"),  s8("*)")    },
        { s8("(a"),  s8("(a")    },
        { s8("(("),  s8("((")    },
        { s8(")a"),  s8(").a")   },
        { s8(")|"),  s8(")|")    },
        { s8(")*"),  s8(")*")    },
        { s8(")("),  s8(").(")   },
        { s8("))"),  s8("))")    },
    };
    bool passed = true;

    for (size i = 0; i <= lengthof(tcs); i++) {
        bool res = s8cmp(add_concat(a, tcs[i].in), tcs[i].expected);
        passed = passed && res;
        if (!res) {
            append_cstr(&out, "add concat test ");
            append_size(&out, i + 1);
            append_cstr(&out, " failed\n");
        }
    }
    return passed;
}

static bool test_to_postfix(struct arena *a)
{
    struct test_case {
        struct s8 in;
        struct s8 expected;
        bool res;
    };
    struct test_case tcs[] = {
        { s8("a"),          s8("a"),       true   },
        { s8("a.b"),        s8("ab."),     true   },
        { s8("a|b"),        s8("ab|"),     true   },
        { s8("a*"),         s8("a*"),      true   },
        { s8("a.b|c"),      s8("ab.c|"),   true   },
        { s8("a|b.c"),      s8("abc.|"),   true   },
        { s8("a.b*"),       s8("ab*."),    true   },
        { s8("a*.b"),       s8("a*b."),    true   },
        { s8("a|b*"),       s8("ab*|"),    true   },
        { s8("(a)"),        s8("a"),       true   },
        { s8("(a.b)*"),     s8("ab.*"),    true   },
        { s8("a.(b|c)"),    s8("abc|."),   true   },
        { s8("(a|b).c"),    s8("ab|c."),   true   },
        { s8("((a))"),      s8("a"),       true   },
        { s8("(a.b|c)*"),   s8("ab.c|*"),  true   },
        { s8("(a|(b.c))*"), s8("abc.|*"),  true   },
        { s8("a.b.c"),      s8("ab.c."),   true   },
        { s8("a|b|c"),      s8("ab|c|"),   true   },
        { s8("a.b|c.d"),    s8("ab.cd.|"), true   },
        { s8("a|b.c|d"),    s8("abc.|d|"), true   },
        { s8("(a.b"),       s8(""),        false  },
        { s8("a.b)"),       s8(""),        false  },
        { s8("a.b))"),      s8(""),        false  },
        { s8("((a.b)"),     s8(""),        false  },
    };
    bool passed = true;

    for (size i = 0; i <= lengthof(tcs); i++) {
        struct s8 pre;
        bool res = to_postfix(a, tcs[i].in, &pre) == tcs[i].res;
        if (res && tcs[i].res) {
            res = s8cmp(pre, tcs[i].expected);
        }
        passed = passed && res;
        if (!res) {
            append_cstr(&out, "to postfix ");
            append_size(&out, i + 1);
            append_cstr(&out, " failed\n");
        }
    }
    return passed;
}
