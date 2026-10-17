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

**Pitfall: a space separator.** The separator of `%` gets `<.ws>` on both
sides, and the implicit whitespace after an atom is consumed greedily (PEG
does not backtrack), so a single space can disappear before the separator is
tried:

```raku
rule TOP { <word>+ % ' ' }     # fails on "one two": the space is eaten as whitespace
rule TOP { <word>+ % ',' }     # fine
token TOP { <word>+ % ' ' }    # fine: a token has no implicit whitespace
```

Use a `token`, or a separator that is not whitespace, when the separator
itself is whitespace.

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

## 5.8 Grammars as QScript values

Grammars are values in QScript, like integers and strings. A program can
declare them, pass them to functions, derive new ones at run time, compile
text into a grammar, and walk the match trees they produce. This is the main
reason QScript exists: a parser is written as a grammar, and the program
works on its result.

### Declaring a grammar

```qscript
grammar Base {
    token num { \d+ }
    token ws  { \s* }
}
grammar Calc is Base {
    rule TOP  { <expr> }
    rule expr { <term>+ % '+' }
    token term { <num> | '(' ~ ')' <expr> }
}
```

* `grammar NAME [is PARENT] { ... }` declares a grammar at the top level of
  a source. The body uses the grammar language of this chapter: `rule`,
  `token`, `regex`, `proto`, aliases, `%` separators, lookaheads.
* A grammar is checked when the program is compiled; an error in the body is
  a compile diagnostic.
* `is PARENT` requires the parent to be declared **earlier in the same
  source**. The child inherits every rule; a rule with the same name in the
  child replaces the parent's.
* The name of a grammar is an expression: `Calc` is a value of type
  `grammar`. A grammar literal `grammar { ... }` is an anonymous one.
* `g.parse(text)` starts at the rule `TOP`. A grammar without a `TOP` rule
  traps when it is parsed without an explicit rule name.

### Parsing and matches

```qscript
let m = Calc.parse("1+2+(3+4)");       # parse from TOP
let n = Calc.parse("3+4", "expr");     # parse from another rule
if (m.ok()) { say m.find_all("num").len(); }
else { say m.error(); }
```

`parse` never fails the program when the input does not match: it returns a
`match` whose `ok()` is `0`. Chapter 11 lists every method on grammars and
matches.

Grammars are passed like any other value. This function accepts any grammar:

```qscript
fn words(g: grammar, text: str): list<str> {
    let found: list<str> = [];
    let m = g.parse(text);
    if (!m.ok()) { return found; }
    for (my w in m.find_all("word")) { found.push(w.text()); }
    return found;
}
```

### Walking a match tree

Values are computed by walking the tree, by rule name. This evaluates the
arithmetic grammar from `tests/qscript-language/grammar_eval.qs`:

```qscript
fn value(m: match): int {
    if (m.rule() == "TOP") { return value(m.child("sum")); }
    if (m.rule() == "num") { return m.text().to_int(); }
    if (m.rule() == "atom") {
        if (m.has("sum")) { return value(m.child("sum")); }
        return value(m.child("num"));
    }
    if (m.rule() == "product") {
        let total = 1;
        for (my a in m.children("atom")) { total *= value(a); }
        return total;
    }
    if (m.rule() == "sum") {
        let total = 0;
        let sign = 1;
        let i = 0;
        while (i < m.size()) {
            let part = m.at(i);
            if (part.rule() == "add-op") { sign = part.text() == "-" ? -1 : 1; }
            else { total += sign * value(part); }
            i++;
        }
        return total;
    }
    return 0;
}
```

Accessors look at direct children (`child`, `has`, `children`, `at`, `size`),
except `find_all`, which searches the whole subtree. `child(name)` traps when
there is no such child, so test with `has(name)` first.

### Changing and creating grammars at run time

```qscript
let loud = Words.extend("token word { <[A..Z]>+ }");   # Words itself is unchanged
let dyn  = compile_grammar("grammar Pair { rule TOP { <n> ',' <n> } token n { \\d+ } }");
say dyn.parse("3,4").ok();
```

* `g.extend(body)` derives a grammar: `body` is rules in the grammar language,
  and each one replaces the rule of the same name in `g` or adds a new one.
  The result has `g`'s name, and `g` is not changed.
* `compile_grammar(source)` compiles a complete grammar declaration from a
  string. A compile error traps the program with `invalid grammar: MESSAGE`,
  where MESSAGE is the grammar engine's message (for example `unknown rule <x>`).
* `g.rules()` lists rule names (inherited ones first) and `g.has_rule(name)`
  tests for one, which is useful before `parse(text, rule)`.

Grammars made this way are ordinary values: they can be passed to functions
and used with every method above.

### Limits and output

Grammar values and match trees live in the run's heap and are subject to its
limits (chapter 11). Grammar evaluation has its own bounds on depth and work
(chapter 9, section 9.7). Printing a grammar gives `grammar NAME`; printing a
match gives its S-expression. Programs that use grammars run on the
interpreter and the JIT, and in generated C99 linked with the QScript runtime
(chapters 10 and 11). Native assembly/object output supports them through the
versioned static parser ABI; `build` includes that parser and static C++/C
libraries in a self-contained Linux x86-64 executable.
