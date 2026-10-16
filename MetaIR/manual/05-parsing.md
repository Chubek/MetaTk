# 5. ParseTk grammars and parsing

## Grammar construction in C++

Register declarations explicitly. C++20 cannot enumerate arbitrary members of
a grammar class, so no API relies on undocumented reflection tricks.

```cpp
parsetk::Registry grammar;
grammar.add(parsetk::Lexeme<"@INT">(parsetk::Pattern::Repo::Integer));
grammar.add(parsetk::Keyword<"$LET">(parsetk::Pattern::Exact("let")));
grammar.add(parsetk::ProductionRule<"#Statement">(
    parsetk::Production::New("$LET @INT | '[' (@INT (',' @INT)*)? ']'")));

parsetk::Parser parser(grammar);
auto result = parser.parse("[1, 2, 3]", "#Statement");
```

Names are exact registry keys, including any `@`, `$`, or `#` prefix. Prefixes
are a useful convention, not a hidden namespace transformation. Distinct names
can refer to identical patterns. `Registry::add_symbol` replaces an existing
declaration with the same name; textual grammar declarations reject duplicates
instead, making accidental redefinition visible in configuration files.

For reusable C++ grammar classes, derive from `Grammar<Derived>` and implement
`define_grammar(Registry&) const`, or implement `GrammarBase::define` directly.
Parser construction takes an owned registry snapshot, so the original grammar
object need not outlive the parser.

## Grammar expression notation

| Form | Meaning |
|---|---|
| `@INT`, `$LET`, `#Rule` | reference an exact registered symbol |
| `'text'`, `"text"` | exact source literal |
| `a b c` | sequence |
| `a / b`, `a | b` | ordered choice |
| `(expression)` | grouping |
| `a*` | zero or more |
| `a+` | one or more |
| `a?` | optional |
| `&a` | positive lookahead |
| `!a` | negative lookahead |
| empty sequence or `''` | empty match |

Choice has lower precedence than sequence; postfix repetition applies to its
preceding atom/group. Prefix lookahead consumes no source and emits no events.
Literal escapes support newline, tab, carriage return, quote, and backslash.
An unmatched parenthesis, invalid escape, undefined symbol, or malformed atom
is a grammar error rather than a source-input mismatch.

PEG choice is ordered: once an alternative succeeds, the parser does not later
revisit that choice merely because a following expression fails. For example,
`('a' | 'ab') 'c'` does not accept `abc`; write the longer alternative first or
factor the grammar. Repetition is greedy and does not backtrack to donate a
successful iteration to later expressions.

## Whitespace and token boundaries

By default the parser skips ASCII space, tab, carriage return, and newline
before terminals and production entries. Set `Options::skip_whitespace=false`
for a whitespace-sensitive grammar. Skipping is not comment preprocessing;
represent source comments explicitly in your grammar when needed.

Named keywords reject a match that would end in an identifier character and
continue into another identifier character. Thus keyword `let` does not match
`letter`. Punctuation keywords are not subjected to a spurious identifier
boundary check. Quoted literals are exact punctuation/text matches and do not
receive named-keyword boundary behavior.

Lexeme patterns are ECMAScript `std::regex` expressions anchored at the current
cursor. They are compiled once at parser construction, not once per token.
`Pattern::Creator::PEG` is a historical spelling for a regex pattern; it is not
a second PEG grammar parser. Use `PatternSpec::regex` in new code when that
distinction matters.

Built-in repositories supply decimal `Integer`, C-style `CIdent`/`Identifier`,
and `Whitespace`. Regex captures are not separate tokens. Empty regex or named
terminal matches are errors because they cannot safely drive repetition.

## Recursion and bounded execution

Right recursion is supported after source progress:

```text
'a' #SameRule?
```

Re-entering the same production at the same input offset is left recursion and
is rejected. This catches direct and indirect non-progress recursion without
incorrectly forbidding all recursion. ParseTk does not implement left-recursion
seed growing or an Earley parser. Rewrite left-recursive arithmetic grammars
as repetition, or use another parsing algorithm if left recursion is essential.

The parser bounds input bytes, expression operations, structural depth, and
pending events. Repetition of an expression that succeeds without advancing the
cursor is rejected immediately. Limits apply to parser operations, not to the
internal work done by `std::regex`; avoid catastrophic-backtracking regexes.

## Events and transaction boundaries

Successful parses emit `RuleBegin`, `Token`, and `RuleEnd` events. Events carry
owned names/text, source spans, structural rule depth, and a synthetic flag
(false for these parser-generated events). Span offsets and lengths are bytes,
not Unicode code points. Lines/columns count bytes with `\n` as the line break.

The parser buffers events while exploring alternatives and lookahead. Failed
branches cannot leak speculative tokens to listeners. A failed complete parse
emits one `ParseError`, not an unmatched sequence of rule-begin events. A
successful parse delivers its committed event sequence only after validation,
including the optional EOF check.

```cpp
parser.subscribe([](const parsetk::Event& e) {
    if (e.kind == parsetk::EventKind::Token) {
        // e.name identifies the token; e.text owns its source spelling.
    }
});
```

Listeners registered by reference must outlive their subscription. Callback
collections are snapshotted during delivery, so subscribing during a callback
does not invalidate the active iteration. Callback exceptions propagate to the
caller; they are application failures, not grammar mismatches. Calling `parse`
again on the same parser during event delivery returns a non-reentrancy error.

## Results and ASTs

`ParseResult` supplies `ok`, `consumed`, `error`, and `error_span`. Parsing
normally requires EOF after optional trailing whitespace. Pass `false` as the
third argument for prefix parsing. Failure coordinates report the retained
farthest mismatch; consumed bytes may reflect backtracking rather than that
farthest position. A parser may be reused after success or failure.

`Toolchain::parse` returns committed events through the unified result type.
`Toolchain::parse_ast` builds a DSLtk tree: productions are inner nodes named by
their rule names, and tokens are leaves tagged by token names with source text
as values. It deliberately does not drop punctuation or infer semantic AST
operators. Perform that lowering in your frontend.

## Textual grammar declarations

```text
lexeme @INT = "[0-9]+"
keyword $LET = "let"
rule #Start = $LET @INT | '[' (@INT (',' @INT)*)? ']'
```

`metachain::text::parse_grammar` returns a registry, default start production,
and error information. The first rule is the default start. Each declaration
occupies one line. Whole-line `//` comments and blank lines are accepted.
Terminal strings use the shared string decoder, so a regex backslash must be
escaped: write `"\\d+"` to convey regex `\d+`. Grammar production bodies are
not string-unquoted a second time.

For the `parse` text command, append a line containing only `%%`, then the
source input. The payload begins on the following line; it is not included in
the grammar definition.

## Scannerless, Raku-style grammars

ParseTk's grammar files are token-based: lexemes are regular expressions and
whitespace is skipped between symbols. For character-level grammars written
in Raku ("Perl 6") syntax, with `token`/`rule`/`proto`, `%` separators,
character classes and lookahead, use the grammar layer in
`qscript/include/QScript-Grammar.h`, which compiles onto ParseTk with
whitespace skipping disabled. See [QScript manual, chapter 5](qscript/05-grammars.md).
`ParseResult::expected`, `expected_span` and `expected_all` report the deepest
terminal failures, including those inside repetitions that later succeeded.

