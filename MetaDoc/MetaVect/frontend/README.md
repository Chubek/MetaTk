# Syntax frontend — milestone 1

## Implemented call graph

```text
driver/mvect-cli.d -> driver/mvect.d: compile(options)
  -> explicit file read -> Input.SourceText
  -> Parser.parse(source) -> Lexer.lex(source) -> Token[]
  -> AST.Module or Diagnostic[] -> driver formats diagnostics
```

`SourceText`, `lex`, and `parse` accept in-memory text and perform no filesystem
access. Import names and function calls are syntax only: no modules are loaded,
no names or types are resolved, and nothing is evaluated. The C/C++ API remains
the existing version/status contract; this tranche exposes no new compiler ABI.

### D module layout

One file defines each module. `frontend/Token.d` and `MetaVect.d` were interface
(`.di`) modules during this milestone's development; both are now `.d` files
because D emits no code for an interface module, and a sibling `.d` makes the
`.di` dead weight. `Visitor.di`, `include/*.di`, and `mvir/MVIR.di` remain
untouched scaffolds. The version and status contract is unchanged: the D, C, and
C++ variants still agree on `0.1.0` and the same status set.

## Source and tokens

- Input is UTF-8 without a BOM. Identifiers are `[A-Za-z_][A-Za-z0-9_]*`.
- `SourceLocation` contains a file name, zero-based byte `offset`, and one-based
  `line`/`column`. Columns count bytes, not Unicode characters or tab stops.
- Spans are half-open. EOF has a zero-width span at `text.length`.
- LF, CRLF, and lone CR are line breaks. Whitespace is space, tab, CR, or LF;
  newlines have no syntactic significance.
- `--` starts a line comment. `{- ... -}` comments nest, without recursive calls.
  Thus `x--1` starts a comment; write `x - -1` for subtraction of a negative value.
- Integers are decimal digits. Floats are digits followed by a fractional part
  and/or exponent: `1.25`, `1e3`, `1.5E-2`. Fractional parts need digits on both
  sides of the dot. Exponents need digits, with an optional sign. Numeric
  suffixes, hex notation, `.5`, and `1.` are not supported.
- Strings are double-quoted, allow UTF-8, and recognize `\n`, `\r`, `\t`, `\"`,
  and `\\`. Raw newlines, control characters, and other escapes are errors.
- Token lexemes and AST literal spellings retain their exact bytes. The parser
  does not decode strings, convert numbers to host values, or fold expressions.

`frontend/Token.d` is the single source of truth for `TokenKind`, `tokenName`,
`keywordKind`, and `Token`. `tokenName` uses a `final switch`, so adding or
renaming a token kind fails to compile until it is named there; `keywordKind`
derives reserved words from `tokenName` so the two cannot drift. The lexer keeps
its operator spellings in a private longest-match table and has no other copy of
the grammar.

## Grammar subset

```ebnf
source       = ["module", qualified, [";"]],
               {"import", qualified, [";"]}, {binding, [";"]} ;
qualified    = identifier, {".", identifier} ;
binding      = ["export"], ["let"], identifier, [":", type], "=", expression ;
type         = type_atom, ["->", type] ;
type_atom    = identifier | "(", type, ")" ;
parameter    = identifier, [":", type] ;
parameters   = [parameter, {",", parameter}] ;
arguments    = [expression, {",", expression}] ;

(* Operator precedence is specified below, not by this abbreviated production. *)
expression   = identifier | literal | "(", ")" | "(", expression, ")"
             | ("+" | "-" | "!"), expression
             | expression, binary_op, expression
             | expression, "(", arguments, ")"
             | ("fn" | "fun"), "(", parameters, ")", "=>", expression
             | "let", identifier, [":", type], "=", expression, "in", expression
             | "if", expression, "then", expression, "else", expression ;
literal      = integer | float | string | "true" | "false" ;
```

All binary operators are syntactically left-associative. Precedence, from low
to high:

| Operators | Role |
| --- | --- |
| `\|\|` | Boolean or |
| `&&` | Boolean and |
| `== !=` | Equality |
| `< <= > >=` | Comparison |
| `+ -` | Addition/subtraction |
| `* /` | Multiplication/division |
| unary `+ - !` | Prefix operators |
| `f(...)` | Calls, including chained calls |

Lambda bodies, local-let bodies, and conditional branches consume complete
expressions. Parenthesize a compound expression before applying an operator or
call to the entire expression. Function-type arrows associate to the right.
An optional single semicolon separates declarations; newlines do not terminate
expressions. Trailing commas and standalone top-level expressions are rejected.
`()` is a unit literal; tuple and collection literals are not yet implemented.

`type`, `forall`, `match`, and `with` are reserved tokens for later milestones,
but their constructs are currently rejected. Brackets, braces, `::`, `|`, `&`,
and `->` outside types are likewise not accepted expression syntax. Built-in
type names are ordinary identifiers, not a hard-coded backend dependency.

## API contracts and errors

- `lex(SourceText)` returns `LexResult { tokens, diagnostics }`. A non-null
  source is required. Tokens always end in one EOF token. Lexing stops at the
  first error; failed token streams must not be used as a valid program.
- `parse(SourceText)` returns `ParseResult { ast, diagnostics }`. It lexes first
  and returns either a complete module with no diagnostics, or a null AST with
  one diagnostic. Empty/comment-only sources are valid anonymous modules.
- Diagnostics carry a kind, message, and source span. `formatDiagnostic` renders
  `file:line:column: error: message`; unexpected-token errors state the expected
  construct and actual token, including EOF.
- Expression/type recursion is capped at `maxSyntaxDepth` (256 parser frames).
  Excess depth is a diagnostic rather than unbounded recursion. Flat binary and
  call chains, source mapping, and nested comment scanning are iterative.
- Resource failures (such as allocation failure) are not disguised as syntax
  errors. Invalid `SourceText` offset/span requests and a null source are API
  precondition errors, not source diagnostics.

The AST records all expressions, declaration metadata, type syntax, and source
spans without committing to semantic meaning. Missing imports, unbound names,
duplicate declarations, type mismatches, and out-of-range numeric values are
not checked yet. `Semacheck-Visitor.d`, `Symtbl.d`, and `Visitor.di` remain
scaffolds; their next contract is name/type validation before MVIR lowering.

## Validation

CTest exercises source mapping, all token kinds, malformed UTF-8, nested
comments, string/number failures, AST structure, precedence/associativity,
declarations, type syntax, nesting limits, explicit not-implemented compilation,
CLI exit codes, reproducible malformed token streams, and every existing
standard-library `.mvct` interface. Assertions in the D tests use runtime checks
even in release builds. When Vim is installed, an additional test checks editor
token groups, nested comments, and escaped-string boundaries.
