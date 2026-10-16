# 5. Grammars

QScript's parser is not hand-written. It is a grammar in a Raku-style
("Perl 6") grammar language, compiled at startup onto MetaChain's ParseTk PEG
engine. The same engine is available to you: from C++
(`qscript::grammar::Grammar`), from C (`qs_grammar_*`), and from the command
line (`qscript parse`).

```raku
grammar Calc {
    rule  TOP    { <expr> }
    rule  expr   { <term>+ % <op> }
    token term   { <num> | '(' ~ ')' <expr> }
    token num    { \d+ }
    proto token op {*}
    token op:sym<+>  { <sym> }
    token op:sym<->  { <sym> }
    token op:sym<**> { <sym> }
    token op:sym<*>  { <sym> }
}
```

```sh
$ echo '1 + 2 ** (3 - 4)' > in.txt
$ qscript parse calc.grammar in.txt
(TOP (expr (term (num "1")) (op:+ "+") (term (num "2")) (op:** "**") (term (expr (term (num "3")) (op:- "-") (term (num "4"))))))
```

## 5.1 Scannerless parsing on ParseTk

ParseTk is an event-driven PEG engine whose terminals are literals and
regular expressions, and which normally skips whitespace between symbols.
The grammar layer turns it into a **scannerless** parser:

* whitespace skipping is switched off, so every character is matched by the
  grammar itself;
* each declaration becomes a ParseTk production; literals stay literals;
  character classes become regex lexemes (matched by a byte-table fast path
  rather than `std::regex` when they are plain classes);
* `rule` bodies get explicit `<.ws>` calls where the source has whitespace;
* the resulting ParseTk event stream (rule begin/end) is folded into a match
  tree of named captures.

Because nothing is tokenized up front, different parts of a grammar can have
different lexical rules (QScript's character literals, numbers with `_`,
comments and keywords are all just rules), and grammars compose by
inheritance.

`qs_grammar_peg` / `Grammar::peg()` show the generated ParseTk grammar:

```
rule .ws = ws
rule TOP = (expr .ws)
rule expr = ((term ((.ws op .ws term))*) .ws)
lexeme #L0 = /(?:[0-9])+/
rule num = #L0
rule op#0 = '+'
...
rule op = (op#2 | op#0 | op#1 | op#3)
lexeme #L2 = /[ \t\r\n\f\v]+/
rule ws = (#L2)?
```

## 5.2 Grammar files

```raku
# comments run to end of line
grammar Base {
    token num { \d+ }
}
grammar Calc is Base {        # inherit Base's declarations, then add/override
    rule TOP { <num>+ % ',' }
}
```

A source may contain several grammars; `is PARENT` copies an earlier
grammar's declarations, and a declaration with the same name (and `sym`)
replaces the inherited one. The **last** grammar in the source is the one
compiled. Parsing starts at `TOP` unless another rule is named.

Declarations:

| Form | Meaning |
|------|---------|
| `token NAME { ... }` | match exactly as written; whitespace in the body is insignificant |
| `rule NAME { ... }` | like `token`, but whitespace after an atom in the body matches `<.ws>` |
| `regex NAME { ... }` | accepted as a synonym of `token` (PEG matching never backtracks into a match) |
| `proto token NAME {*}` | declares a set of alternatives `NAME:sym<...>` |
| `token NAME:sym<S> { ... }` | one alternative of proto NAME; `<sym>` in the body matches `S` |

Rule names are identifiers that may contain `-` (`kw-if`). Use `:sym«...»`
when the symbol contains `>`: `token infix:sym«>=» { <sym> }`.

## 5.3 Regex syntax

| Syntax | Matches |
|--------|---------|
| `'text'`, `"text"` | the literal text (escapes `\n \t \r \0 \\ \' \"`) |
| `abc` (bare word characters) | the literal `abc` |
| `\d \w \s \h \v \N` | digit, word char, whitespace, horizontal / vertical space, non-newline |
| `\D \W \S \H \V` | the complements |
| `\n \t \r \e \0 \xHH \x[HH]` | single characters |
| `\.` `\(` ... | escaped punctuation, literally |
| `.` | any character |
| `<[a..z A..Z _]>` | character class: characters and ranges, whitespace ignored |
| `<-[ \n ]>` | negated class |
| `<+alpha -[_]>`, `<[a..f]+digit>` | class arithmetic with `+` / `-` |
| `<alpha> <digit> <alnum> <upper> <lower> <xdigit> <space> <blank> <cntrl> <punct>` | named classes (captured) |
| `<ident>` | `<alpha> \w*` (captured) |
| `<ws>` | optional whitespace, unless the grammar defines its own `ws` |
| `<name>` | call rule `name` and capture it |
| `<.name>` | call rule `name` without capturing |
| `<alias=name>`, `<alias=.name>` | call `name`, capture it as `alias` |
| `<sym>` | inside `NAME:sym<S>`: the literal `S` |
| `[ ... ]`, `( ... )` | grouping (no positional captures) |
| `a b` | sequence |
| `a \| b`, `a \|\| b` | ordered choice (first match wins) |
| `X*  X+  X?` | zero or more / one or more / optional (greedy, possessive) |
| `X ** 3`, `X ** 2..5`, `X ** 2..*` | counted repetition |
| `X+ % SEP`, `X* % SEP` | X separated by SEP |
| `X+ %% SEP` | the same, allowing a trailing SEP |
| `X*?  X+?  X??` | frugal: stop as soon as what follows matches (`'/*' .*? '*/'`) |
| `OPEN ~ CLOSE INNER` | goal match: `OPEN INNER CLOSE` (`'(' ~ ')' <expr>`) |
| `<?before X>`, `<!before X>` | positive / negative lookahead |
| `<?name>`, `<!name>` | lookahead for a rule |
| `<?>`, `<!>` | always succeed / always fail |
| `>>` or `»` | right word boundary (no word character follows) |
| `$` | end of input; `$$` end of line |
| `^`, `^^`, `<<`, `«` | accepted and ignored (see 5.6) |
| `# ...` | comment inside a body |

Unquoted punctuation other than the metacharacters above is an error, as in
Raku: write `'+'`, not `+`.

### `rule` and whitespace

In a `rule`, whitespace in the body *after an atom* (including after the
last atom) becomes `<.ws>`. Leading whitespace is ignored. Separators get
`<.ws>` on both sides:

```raku
rule pair { <key> '=' <value> }        # key .ws '=' .ws value .ws
rule list { '[' <item>* % ',' ']' }     # items may be separated by ", " or " , "
```

The default `ws` is optional whitespace. Define your own to add comments:

```raku
token ws { [ \s+ | '#' \N* ]* }
```

## 5.4 Protos and longest-token ordering

PEG alternation is ordered: `'<' | '<='` never matches `<=` as a whole. Raku
uses longest-token matching instead. The grammar layer approximates it for
protos: the variants of a proto are tried in order of **decreasing literal
prefix length**, keeping declaration order for ties. So `op:sym<**>` is tried
before `op:sym<*>`, and `infix:sym«<=»` before `infix:sym«<»`, whatever the
declaration order. Plain `|` alternations stay strictly ordered; put longer
alternatives first or use a proto.

A variant appears in the match tree under the proto's name with its `sym`
recorded: `(op:** "**")`.

## 5.5 The match tree

A successful parse returns a tree of `Match` (`qs_match`) nodes:

| Field | Meaning |
|-------|---------|
| `name` | the rule (or alias) name |
| `sym` | for proto variants, the symbol; otherwise empty |
| `text` | the matched input |
| `from`, `to` | byte offsets |
| `line`, `column` | 1-based position of `from` |
| `children` | named sub-captures, in order |

Literals and character classes are not captured; `<.name>` suppresses a
capture; an alias renames one. `dump()` prints an S-expression where leaves
show their text and proto variants show as `name:sym`.

```cpp
#include "qscript/include/QScript-Grammar.h"
using qscript::grammar::Grammar;

auto g = Grammar::compile(source_text);
if (!g.ok()) { std::cerr << g.error << "\n"; return; }
auto r = g.parse("a=1, b=-2", "TOP");
if (!r) { std::cerr << r.line << ":" << r.column << ": " << r.error << "\n"; return; }
for (const auto* pair : r.match->all("pair"))
    std::cout << pair->find("key")->text << " -> " << pair->find("value")->text << "\n";
```

Grammars can also be built in code:

```cpp
Grammar h;
h.name = "Pairs";
h.rule("TOP", "<pair>+ % ','")
 .token("pair", "<key=ident> '=' <value=num>")
 .token("num", "'-'? \\d+");
if (!h.build()) std::cerr << h.error;
```

`Match::to_ast()` converts a match tree into a `dsl::ASTNode` (tags are
`name` or `name:sym`, leaves carry the text), for use with the rest of
MetaChain.

### Errors

On failure the outcome carries `error`, `line`, `column`, `offset`, the list
of `expected` alternatives at the furthest point reached, and the `found`
input there. ParseTk was extended to remember the deepest failure even
inside repetitions that later succeeded, so the report points at the real
problem: for `1 +` the message is `expected \d+ or '('` at column 4, not
"unexpected trailing input" at column 3.

Limits: input size (64 MiB), evaluation depth (4096) and operation count
(2 million plus 512 per input byte) are bounded; exceeding them is an error,
not a crash.

## 5.6 Differences from Raku

* Matching is PEG: quantifiers are greedy and possessive, alternation is
  ordered, and nothing backtracks into a completed match. `regex` behaves like
  `token`. Frugal quantifiers work by looking at what follows them.
* Longest-token matching is approximated for protos only (5.4).
* No embedded code blocks (`{ ... }`), actions classes, `make`/`made`,
  arguments (`<name(...)>`), adverbs (`:i`, `:s`, `:r`), positional captures,
  `<?after>` (needs lookbehind), Unicode properties, or conjunctions (`&`).
  Build values by walking the match tree.
* `<<` (left word boundary) and `^`/`^^` are accepted but do nothing; a parse
  always starts at offset 0 and must consume the whole input.
* The default `<ws>` does not implement `<!ww>`; in a `rule`, two adjacent
  word-like literals may match without whitespace between them.
* Character classes are byte-oriented (ASCII ranges; UTF-8 passes through as
  bytes).

## 5.7 The QScript grammar

`qscript grammar` prints the grammar QScript itself is parsed with
(`qscript/grammar/QScript.grammar`). Highlights:

```raku
rule TOP        { <.ws> <item>* }
rule item       { <use> | <fn> | <const> | <statement> }

proto rule statement {*}
rule statement:sym<if>     { <.kw-if> <cond> <then=statement> <elsif>* [ <.kw-else> <else=statement> ]? }
rule statement:sym<print>  { [ <.kw-print> | <.kw-say> ] <expr>+ % ',' <modifier>? ';' }
...
rule binary     { <operand>+ % <infix> }          # flat: precedence is applied afterwards
rule operand    { [ <prefix> ]* <term> }
proto token infix {*}
token infix:sym<**>  { <sym> <!before '='> }
token infix:sym«<=»  { <sym> }
...
token ident     { <!keyword> <[A..Za..z_]> \w* }
token ws        { [ \s+ | '//' \N* | '#' \N* | '/*' .*? '*/' ]* }
token kw-if     { 'if' >> }
```

Binary expressions are matched as flat operand/operator lists and turned
into trees by an operator-precedence pass (shunting-yard) in
`qscript/src/AST.cpp`, the way Raku's own EXPR works. That keeps the grammar
short and makes adding an operator a matter of one proto variant plus one
precedence entry. `qscript tree FILE` shows the match tree for a program and
`qscript ast FILE` the AST built from it.
