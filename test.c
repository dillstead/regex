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
        { s8("a"),               s8("a\0"),             true  },  // a
        { s8("a"),               s8("b\0"),             false },  // a
        // empty pattern and empty input
        { s8(""),                s8("\0"),              true  },  // (empty)
        { s8(""),                s8("abc\0"),           true  },  // (empty)
        { s8("a"),               s8("\0"),              false },  // a
        { s8("a*"),              s8("\0"),              true  },  // a*
        // literals: case sensitive, digits, space
        { s8("a"),               s8("A\0"),             false },  // a
        { s8("Z"),               s8("Z\0"),             true  },  // Z
        { s8("7"),               s8("x7y\0"),           true  },  // 7
        { s8(" "),               s8("a b\0"),           true  },  // (space)
        { s8(" "),               s8("ab\0"),            false },  // (space)
        // unanchored: match at start, middle, end
        { s8("a"),               s8("abc\0"),           true  },  // a
        { s8("b"),               s8("abc\0"),           true  },  // b
        { s8("c"),               s8("abc\0"),           true  },  // c
        { s8("d"),               s8("abc\0"),           false },  // d
        // concatenation
        { s8("ab.c."),           s8("abc\0"),           true  },  // abc
        { s8("ab.c."),           s8("xxabcxx\0"),       true  },  // abc
        { s8("ab.c."),           s8("ab\0"),            false },  // abc
        { s8("ab.c."),           s8("acb\0"),           false },  // abc
        { s8("ab.c."),           s8("ab c\0"),          false },  // abc
        // restart after a partial match
        { s8("aa.b."),           s8("aaab\0"),          true  },  // aab
        { s8("ab.a.b."),         s8("abaabab\0"),       true  },  // abab
        { s8("ab.a.b."),         s8("abaaba\0"),        false },  // abab
        // alternation
        { s8("ab|"),             s8("a\0"),             true  },  // a|b
        { s8("ab|"),             s8("b\0"),             true  },  // a|b
        { s8("ab|"),             s8("c\0"),             false },  // a|b
        { s8("ab|c|"),           s8("xxc\0"),           true  },  // a|b|c
        { s8("ab.c.ab.d.|"),     s8("abd\0"),           true  },  // abc|abd
        { s8("ab.c.ab.d.|"),     s8("abe\0"),           false },  // abc|abd
        { s8("ab.cd.|"),         s8("acbd\0"),          false },  // ab|cd
        { s8("abc|.d."),         s8("acd\0"),           true  },  // a(b|c)d
        { s8("abc|.d."),         s8("abd\0"),           true  },  // a(b|c)d
        { s8("abc|.d."),         s8("ad\0"),            false },  // a(b|c)d
        { s8("abc|.d."),         s8("abcd\0"),          false },  // a(b|c)d
        // closure
        { s8("a*"),              s8("bbb\0"),           true  },  // a*
        { s8("ab*.c."),          s8("ac\0"),            true  },  // ab*c
        { s8("ab*.c."),          s8("abc\0"),           true  },  // ab*c
        { s8("ab*.c."),          s8("abbbbbbbbc\0"),    true  },  // ab*c
        { s8("ab*.c."),          s8("abbbbdc\0"),       false },  // ab*c
        { s8("ab*.c."),          s8("ab\0"),            false },  // ab*c
        { s8("xab.*.c."),        s8("xc\0"),            true  },  // x(ab)*c
        { s8("xab.*.c."),        s8("xababc\0"),        true  },  // x(ab)*c
        { s8("xab.*.c."),        s8("xabac\0"),         false },  // x(ab)*c
        { s8("xab|*.c."),        s8("xabbac\0"),        true  },  // x(a|b)*c
        { s8("xab|*.c."),        s8("xab\0"),           false },  // x(a|b)*c
        { s8("xab|*.y."),        s8("xababababbbay\0"), true  },  // x(a|b)*y
        { s8("xab|*.y."),        s8("xabzy\0"),         false },  // x(a|b)*y
        // nested, and paths that share list entries
        { s8("xab|cd|.*.e."),    s8("xacbde\0"),        true  },  // x((a|b)(c|d))*e
        { s8("xab|cd|.*.e."),    s8("xacbe\0"),         false },  // x((a|b)(c|d))*e
        { s8("xaa.a|*.b."),      s8("xaaaaab\0"),       true  },  // x(aa|a)*b
        { s8("xaa.a|*.b."),      s8("xaaaaa\0"),        false },  // x(aa|a)*b
        { s8("xa*.a*.a*.b."),    s8("xaaaaaaaab\0"),    true  },  // xa*a*a*b
        { s8("xa*.a*.a*.b."),    s8("xaaaaaaaa\0"),     false },  // xa*a*a*b
        { s8("xaab.|.cbc.d.|."), s8("xabcd\0"),         true  },  // x(a|ab)(c|bcd)
        { s8("xaab.|.cbc.d.|."), s8("xabd\0"),          false },  // x(a|ab)(c|bcd)
        { s8("xa*b.*.c."),       s8("xabaabbc\0"),      true  },  // x(a*b)*c
        { s8("xa*b.*.c."),       s8("xabaac\0"),        false },  // x(a*b)*c
        // match ending on the last character before the terminator
        { s8("ab."),             s8("xxab\0"),          true  },  // ab
        { s8("ab*."),            s8("xxabbb\0"),        true  },  // ab*
        // embedded NUL does not end the line
        { s8("b"),               s8("a\0b\0"),          true  },  // b
        { s8("ab."),             s8("a\0b\0"),          false },  // ab
    };
    bool passed = true;

    for (size i = 0; i <= lengthof(tcs); i++) {
        b32 (*match)(const u8 *, size) = arch_compile(scratch, tcs[i].re);
        bool res = match != NULL;
        if (res) {
            res = match(tcs[i].input.data, tcs[i].input.len) == tcs[i].expected;
        }
        passed = passed && res;
        if (!res) {
            append_cstr(&out, "match test ");
            append_size(&out, i + 1);
            append_cstr(&out, " failed\n");
        }
    }
    return passed;
}
