# Chapter 36 — Explicit Context-Free Grammars and Earley Parsing

`CFG` separates grammar productions from lexing. A user lexer produces a
sequence of string tokens; named productions describe how those tokens form
an AST. The Earley parser handles left recursion, nullable productions and
unordered alternatives.

## Symbols and productions

`CFGSymbol` distinguishes a terminal token from a nonterminal name.
`CFGRule` stores a left-hand-side name and a right-hand-side symbol sequence.
`CFG` stores `start` and `productions` and offers `add`, `validate` and `parse`.

```cpp
dsl::CFG grammar;
grammar.start = "Expr";
grammar.add("Expr", {dsl::CFGSymbol::nonterminal("Expr"),
                     dsl::CFGSymbol::token("+"),
                     dsl::CFGSymbol::nonterminal("Term")});
grammar.add("Expr", {dsl::CFGSymbol::nonterminal("Term")});
grammar.add("Term", {dsl::CFGSymbol::token("number")});
grammar.validate();
assert(grammar.parse({"number", "+", "number"}).ok());
```

```d
CFG grammar;
grammar.start = "Expr";
grammar.add("Expr", [CFGSymbol.nonterminal("Expr"),
                     CFGSymbol.token("+"), CFGSymbol.nonterminal("Term")]);
grammar.add("Expr", [CFGSymbol.nonterminal("Term")]);
grammar.add("Term", [CFGSymbol.token("number")]);
grammar.validate();
assert(grammar.parse(["number", "+", "number"]).ok());
```

Terminals compare exactly against string tokens. `"number"` in this example
is a literal token value supplied by the lexer; it is not a built-in token
class or regex. The user's lexer can normalize numeric input to that token,
or construct a grammar using its own concrete token values.

Validation requires nonempty production names, a defined start symbol, and
definitions for every referenced nonterminal. `parse` validates before running.

## Read the grammar from S-expressions

```lisp
(grammar
    (start Expr)
    (rule Expr (Expr "+" Term) (Term))
    (rule Term ("number")))
```

```cpp
auto grammar = dsl::sexpr_grammar(R"((grammar
    (start Expr)
    (rule Expr (Expr "+" Term) (Term))
    (rule Term ("number"))))");
```

```d
auto grammar = sexpr_grammar(`(grammar
    (start Expr)
    (rule Expr (Expr "+" Term) (Term))
    (rule Term ("number")))`);
```

The declaration grammar is:

```ebnf
grammar = "(grammar" , start , { rule } , ")" ;
start = "(start" , name , ")" ;
rule = "(rule" , name , alternative , { alternative } , ")" ;
alternative = "(" , { symbol } , ")" ;
symbol = nonterminal_name | quoted_terminal ;
```

The notation permits declaration ordering independent of references: all
references are validated after construction. Unquoted RHS atoms are
nonterminals; quoted RHS atoms are terminals. Each RHS list is a separate
alternative. An empty list is an epsilon production.

## Use template-level vocabulary

`CFGTerminal<Text>`, `CFGNonterminal<Name>`, and `CFGProduction<LHS, Symbols...>`
provide C++ template construction. D uses native string arguments and `!`.

```cpp
using Number = dsl::CFGProduction<"Term", dsl::CFGTerminal<"number">>;
using Expression = dsl::CFGProduction<"Expr", dsl::CFGNonterminal<"Term">>;
auto grammar = dsl::make_cfg<"Expr">(Expression{}, Number{});
```

```d
alias Number = CFGProduction!("Term", CFGTerminal!"number");
alias Expression = CFGProduction!("Expr", CFGNonterminal!"Term");
auto grammar = make_cfg!"Expr"(Expression(), Number());
```

`make_cfg` materializes and validates a `CFG`; recognition still runs over
runtime tokens. A production with no symbol types represents epsilon.

## Recognition and result ASTs

Earley chart processing uses prediction, scanning and completion. An item
records its production, dot position and origin token position. Equivalent
chart items are deduplicated, allowing recursive and nullable grammars to reach
a finite chart closure. A completed item carries the first discovered tree.

```cpp
auto parsed = grammar.parse({"number"});
assert(parsed.ok());
assert(parsed.tree->tag() == "Expr");
```

```d
auto parsed = grammar.parse(["number"]);
assert(parsed.ok());
assert(parsed.tree.get.tag() == "Expr");
```

`CFGParseResult` contains an optional `tree`, a token `position`, expected
terminal strings, and `ok()`. On success, `position` equals the input token
count. Production names become inner-node tags; terminal leaves use tag
`token` and carry the token's text. No implicit semantic evaluation occurs.

For ambiguous grammars, the parser returns the first discovered derivation.
It does not expose a packed forest of all parses. Use grammar design and
production order deliberately when the selected derivation matters.

## Nullable productions and diagnostics

```lisp
(grammar
    (start S)
    (rule S (A A))
    (rule A () ("a")))
```

This grammar accepts zero, one or two `a` tokens. The parser handles completion
of nullable symbols even when an appropriate waiting item is inserted after
the nullable completion was first discovered.

For the expression grammar above, input `number +` reaches token position 2
and expects terminal `number`. `expected` contains distinct terminal names
from the farthest populated chart. Trailing input can report `<end-of-input>`
when no ordinary terminal expectation explains it. Undefined grammar symbols
raise a validation exception; token recognition failure is a result value.

## Choose the parser semantics explicitly

| Facility | Input/semantics |
|---|---|
| Parser combinators | Callable-based parsers over `ParsecInput`, with soft/committed errors |
| PEG facilities | Ordered choice, spans, lookahead and rule/channel matching |
| `CFG` | Token-level productions, unordered alternatives, left recursion and epsilon |

The same S-expression substrate can hold either equality rules or CFG
declarations, but their readers and semantics are separate. `sexpr_rules`
reads rewrite rules; `sexpr_grammar` reads grammar declarations. A static byte
regex (Chapter 33) can assist a user lexer; it is not a CFG nonterminal.

## Assemble the language pipeline

A practical progression through the toolkit is:

1. Read source through a user I/O backend.
2. Preprocess rule/grammar source and resolve user includes or Lua-generated text.
3. Read the grammar and tokenize the domain source with the user's lexer.
4. Recognize tokens and obtain a tagged AST.
5. Derive a visitor for domain validation, evaluation or lowering.
6. Convert an expression AST to an e-graph and saturate S-expression equalities.
7. Trigger domain rewriters on equality and extract a representative.
8. Serialize the resulting representation with the user's codec/output backend.

The reusable stages are documented in Chapters 32–35; domain vocabulary,
lexing, semantic operations and backend syntax are supplied by the user.
