# 39. FlexBridge, YACCBridge, and Parsec

The parser libraries expose DSLtk-based language-building tools inside MetaML. FlexBridge tokenizes with ordered longest-match rules; YACCBridge parses supported grammar descriptions; Parsec composes typed parser handles. These are libraries, not invocations of external `flex` or `yacc` executables.

```metaml
rules := [("num", "[0-9]+"), ("_ws", "[ \\t\\n]+")]
Test.equal "tokenize" (FlexBridge.tokenize rules "12 34") (Ok [("num", "12"), ("num", "34")])
words := Parsec.sep_by (Parsec.regex "[a-z]+") (Parsec.char ",")
Test.equal "parser composition" (Parsec.run words "a,bc") (Ok ["a", "bc"])
Test.equal "prefix parser" (Parsec.run_prefix (Parsec.string "ab") "abc") (Ok ("ab", "c"))
```

FlexBridge skips rules whose names start with underscore. The longest match wins, with earlier rules breaking ties. Its pattern language includes character classes, simple quantifiers, escapes, and negative lookahead; it is not an unrestricted PCRE implementation. Validate patterns explicitly when they come from users.

YACCBridge accepts productions, alternatives, quoted terminals, and supported token conventions, returning generic Ast trees. Its parser supports tested left-recursive grammar forms, but the grammar reader is not a full replacement for every yacc directive or semantic-action language. Keep semantic interpretation in an AST visitor.

Parsec supplies character/string primitives, sequence and alternatives, repetition, separators, mapping, binding, lazy recursion, labels, and end-of-input checks. `run` requires complete consumption; `run_prefix` returns the unconsumed suffix. Parser failures backtrack, and error reporting tracks a furthest failure position.

Avoid repeating a parser that succeeds without consuming input: a safe combinator must detect nonprogress, and even then such a grammar rarely expresses the intended language. Use `lazy` to defer recursive parser construction rather than recursively building an infinite host object graph.

**Sources:** [`FlexBridge.h`](../metalib/cpp/FlexBridge.h), [`YACCBridge.h`](../metalib/cpp/YACCBridge.h), [`Parsec.h`](../metalib/cpp/Parsec.h).

<!-- nav:begin -->
[Contents](README.md) · [Previous](38-ast-and-visitors.md) · [Next](40-language-tooling.md)
<!-- nav:end -->
