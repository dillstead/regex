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
        [' '] = true,  ['('] = true, [')'] = true, ['|'] = true,
        ['*'] = true,
    };
    bool passed = true;

    for (size i = 0; i < countof(allowed_chars); i++) {
        bool res = is_valid_char((u8) i) == allowed_chars[i];
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
        { s8(""),       true  },
        // single characters
        { s8("a"),      true  },
        { s8("Z"),      true  },
        { s8("0"),      true  },
        { s8(" "),      true  },
        { s8("|"),      false },
        { s8("*"),      false },
        { s8("("),      false },
        { s8(")"),      false },
        // character whitelist: anywhere in the pattern
        { s8("a b"),    true  },
        { s8("a#b"),    false },
        { s8("a.b"),    false },
        { s8("a\tb"),   false },
        { s8("a\nb"),   false },
        { s8("a\xff" "b"), false },
        { s8("#ab"),    false },
        { s8("ab#"),    false },
        // leading operator
        { s8("|a"),     false },
        { s8("*a"),     false },
        { s8(")a"),     false },
        { s8("(a"),     true  },
        // trailing operator
        { s8("a|"),     false },
        { s8("a("),     false },
        { s8("a*"),     true  },
        { s8("a)"),     true  },
        // adjacent pairs, wrapped in operands so only the pair is tested
        { s8("aaaa"),   true  },
        { s8("aa|a"),   true  },
        { s8("aa*a"),   true  },
        { s8("aa(a"),   true  },
        { s8("aa)a"),   true  },
        { s8("a|aa"),   true  },
        { s8("a||a"),   false },
        { s8("a|*a"),   false },
        { s8("a|(a"),   true  },
        { s8("a|)a"),   false },
        { s8("a*aa"),   true  },
        { s8("a*|a"),   true  },
        { s8("a**a"),   false },
        { s8("a*(a"),   true  },
        { s8("a*)a"),   true  },
        { s8("a(aa"),   true  },
        { s8("a(|a"),   false },
        { s8("a(*a"),   false },
        { s8("a((a"),   true  },
        { s8("a()a"),   false },
        { s8("a)aa"),   true  },
        { s8("a)|a"),   true  },
        { s8("a)*a"),   true  },
        { s8("a)(a"),   true  },
        { s8("a))a"),   true  },
        // whole patterns
        { s8("(a|b)*c"),     true  },
        { s8("a(b|c d)*e"),  true  },
        { s8("(a|)b"),       false },
        { s8("a|*b"),        false },
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

static bool test_add_concat(struct arena scratch)
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
        bool res = s8cmp(add_concat(&scratch, tcs[i].in), tcs[i].expected);
        passed = passed && res;
        if (!res) {
            append_cstr(&out, "add concat test ");
            append_size(&out, i + 1);
            append_cstr(&out, " failed\n");
        }
    }
    return passed;
}

static bool test_to_postfix(struct arena scratch)
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
        bool res = to_postfix(&scratch, tcs[i].in, &pre) == tcs[i].res;
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

static bool test_check_closures(struct arena scratch)
{
    struct test_case {
        struct s8 in;
        bool expected;
    };
    // Inputs are postfix, infix in comments.
    struct test_case tcs[] = {
        { s8("a"),            true  },  // a
        { s8("a*"),           true  },  // a*
        { s8("a*b*."),        true  },  // a*b*
        { s8("ab|*"),         true  },  // (a|b)*
        { s8("ab.*"),         true  },  // (ab)*
        { s8("a*b.*"),        true  },  // (a*b)*
        { s8("ab*.*"),        true  },  // (ab*)*
        { s8("ab|*c*."),      true  },  // (a|b)*c*
        { s8("ab*|c.*"),      true  },  // ((a|b*)c)*
        { s8("ab*c*|.*"),     true  },  // (a(b*|c*))*
        { s8("ab.*c*."),      true  },  // (ab)*(c*)
        { s8("a**"),          false },  // (a*)*
        { s8("ab*|*"),        false },  // (a|b*)*
        { s8("a*b|*"),        false },  // (a*|b)*
        { s8("a*b*.*"),       false },  // (a*b*)*
        { s8("a*bc*|.*"),     false },  // ((a*)(b|c*))*
        { s8("xa**.y."),      false },  // x((a*)*)y
        { s8("xab*c*.|*.y."), false },  // x(a|(b*c*))*y
    };
    bool passed = true;

    for (size i = 0; i <= lengthof(tcs); i++) {
        bool res = check_closures(scratch, tcs[i].in) == tcs[i].expected;
        passed = passed && res;
        if (!res) {
            append_cstr(&out, "check closures test ");
            append_size(&out, i + 1);
            append_cstr(&out, " failed\n");
        }
    }
    return passed;
}

static bool test_match(struct arena scratch)
{
    struct test_case {
        struct s8 re;
        struct s8 input;
        b32 expected;
    };
    struct test_case tcs[] = {
        { s8("a"), s8("a\0"), true },
        { s8("a"), s8("b\0"), false },
    };
    bool passed = true;

    for (size i = 0; i <= lengthof(tcs); i++) {
        b32 (*match)(const u8 *, size) = arch_compile(scratch, tcs[i].re);
        bool res = match != NULL;
        passed = passed && res;
        if (res) {
            res = match(tcs[i].input.data, tcs[i].input.len) == tcs[i].expected;
            passed = passed && res;
            if (!res) {
                append_cstr(&out, "match test ");
                append_size(&out, i + 1);
                append_cstr(&out, " failed\n");
            }
        }
    }
    return passed;
}
