# Authoring Guide — *DSLtk: The Definitive Guide*

This file governs every chapter file in `chapters/`. Read it before writing.

---

## 1. What this book is

A forty-two chapter, technically rigorous book about **DSLtk**, a header-only
Domain-Specific Language construction toolkit. The subject of the book is the
shipped source:

| File | Role | Lines |
|---|---|---|
| `DSLtk/DSLtk.hpp` | The C++20 implementation. **Primary subject.** | 12,024 |
| `DSLtk/DSLtk.d` | The D port. **Secondary subject.** | 3,904 |

`DSLtk.hpp` embeds the complete public headers of `magic_enum` v0.9.8 between
the markers `BEGIN DSLTK EMBEDDED MAGIC_ENUM` (line 403) and
`END DSLTK EMBEDDED MAGIC_ENUM` (line 5816). `namespace dsl` runs from line
5818 to line 12024.

**Everything you write must be true of the shipped code.** When the shipped
code differs from the older `DSLtk/manual/` prose, the shipped code wins. Do
not import claims from `DSLtk/manual/` without checking the header. The most
frequent divergences are listed in Appendix A of this guide.

---

## 2. Output format

Each chapter is a standalone LaTeX file `chapters/chNN-slug.ltx` and begins
with

```latex
\chapter{Title of the Chapter}
\label{ch:slug}
```

and ends with the chapter text. **No** `\documentclass`, no preamble, no
`\begin{document}`. The preamble lives in `main.ltx`; the shared macros live
in `macros.ltx`.

### 2.1 Macros you may use

Defined in `macros.ltx`:

* Text: `\DSLtk`, `\cpp`, `\dlang`, `\cstd`, `\dsltk`, `\dslns`.
* Inline code: `\code{foo}` (monospaced, breakable), `\api{dsl::foo}`,
  `\kwd{...}`, `\codecomment{...}`, `\type{...}`.
* Cross references: `\chapref{label}`, `\secref{label}`, `\figref{label}`,
  `\tabref{label}`, `\lstref{label}`. Use `\cref`/`\Cref` from `cleveref`
  directly when you need plural forms.
* Boxes: `\begin{notebox}[Title]`, `\begin{warnbox}[Title]`,
  `\begin{tipbox}[Title]`, `\begin{featurebox}[Title]`,
  `\begin{csbox}[Title]` (case study summary).
* Pseudocode:
  ```latex
  \begin{pseudocode}[Algorithm title]
  \textbf{function} name (x)\\
  \quad if x > 0 then\\
  \qquad return true\\
  \quad return false
  \end{pseudocode}
  ```
  The tab stops are at 1.5em, 2.7em, 3.9em, 5.1em (four `=` in the
  definition). Use `\\` to end a line and `\quad`/`\qquad` for indentation.
* Listings: `lstlisting` with `[style=dslcpp]`, `[style=dsld]`,
  `[style=dslsexpr]`, `[style=dslcmd]`, `[style=dsldot]`.
  Always give a `caption=` so the listing is numbered and referenceable.
* Figures: `\begin{dslgraph}` ... `\end{dslgraph}` wraps a `tikzpicture`.
  Available TikZ styles: `dslnode`, `dslbox`, `dsllarge`, `dslsmall`,
  `dsldecision`, `dslterm`, `dsledge`, `dsledgedash`, `dslfeedback`,
  `dslgroupr`, `dslgkey`. Node positioning library is loaded
  (`positioning`, `calc`, `shapes.geometric`, `fit`, `backgrounds`).
* Theorem-like: `contract`, `invariant`, `property`, `warning` environments.
* Math: `\Set{}`, `\Seq{}`, `\Mapsto`, `\Step`, `\Steps`, `\Cost{}`, `\FV{}`.

### 2.2 Rules of style

1. **Prose quality.** Write like a serious technical book: complete
   sentences, active voice, no bullet-point soup. Bullets are fine inside
   a table cell or a short list, but a chapter must have real paragraphs
   that explain *why*.
2. **No filler.** Every paragraph must add information a reader could not
   have got by reading the header's identifier names.
3. **Cite line numbers** when referring to the implementation, e.g.
   "the matcher walks the rule list in declaration order
   (\texttt{DSLtk.hpp}, line 8546)". Do this sparingly — maybe two or three
   times per chapter — but do it, because it makes the book verifiable.
4. **Code must compile.** Every C++ listing must be valid C++20 that
   compiles against `DSLtk/DSLtk.hpp`. Prefer short, complete programs.
   If a listing is a fragment, say so in the caption or in the surrounding
   prose ("the following fragment assumes `dsl::QueryDSL` from
   Listing~\ref{...}").
5. **Tables.** At least one `table` environment per chapter, with a
   `caption` and a `\label`. Use `booktabs` rules (`\toprule`,
   `\midrule`, `\bottomrule`). Use the `L{}`/`C{}`/`R{}` column types for
   long text.
6. **Graphs.** At least one TikZ figure per chapter, with a `caption` and a
   `\label`. Prefer figures that show *data flow* or *structure* over
   decorative boxes. Use `\label{fig:...}`.
7. **Pseudocode.** At least one `pseudocode` block per chapter, except in
   chapters whose subject is itself a concrete algorithm listing, where a
   second, higher-level pseudocode summary is still welcome.
8. **Length.** A chapter should be roughly **2,500–4,500 words** of prose
   plus code, tables and figures. Long is good; padded is not.
9. **D material** belongs in its own chapter (Chapter 37) and in clearly
   marked sidebars elsewhere. Never present D syntax as if it were C++.
10. **LaTeX hygiene.** No unescaped `_`, `^`, `&`, `%`, `#`, `$` in prose.
    Use `\code{...}` or `\texttt{...}` for identifiers; escape as
    `\_` inside `\texttt`. Prefer `\code{}` over `\texttt{}` in prose.

### 2.3 Required chapter shape

Every chapter follows roughly this order. Adapt the headings, but keep all
seven elements:

1. **Opening** — 3–6 paragraphs that state the problem the facility
   solves, why it is hard, and what DSLtk's answer is.
2. **A worked example** — a small, complete, compilable program that uses
   the facility. Show the output.
3. **The data model** — the types involved, their members, their lifetimes.
   Use a table.
4. **The algorithm** — how it actually works. Use pseudocode.
5. **A figure** — a graph of the flow.
6. **Edge cases and limits** — what is *not* supported. This is the most
   valuable part of the chapter; be precise and honest.
7. **Closing** — a short synthesis and a pointer to the next chapter.

---

## 3. Chapter map

| # | File stem | Title |
|---|---|---|
| 1 | `ch01-introduction` | Introduction: The Problem of Language Construction |
| 2 | `ch02-getting-started` | Getting Started: Build, Install, First Programs |
| 3 | `ch03-architecture` | Architecture and the Feature Catalogue |
| 4 | `ch04-crtp-foundation` | The CRTP Foundation: `dsl::DSL<Derived, Features...>` |
| 5 | `ch05-fixed-string` | `FixedString`: Compile-Time Strings as Template Parameters |
| 6 | `ch06-feature-tags` | Feature Tags and Mixins: Anatomy of Composition |
| 7 | `ch07-enum-reflection` | Enum Reflection: The Embedded `magic_enum` v0.9.8 |
| 8 | `ch08-pipeline` | The Pipeline Feature |
| 9 | `ch09-operators` | The Operators Feature: Predicate Composition |
| 10 | `ch10-pattern-match` | Compile-Time Dispatch: `match`, `when`, `otherwise` |
| 11 | `ch11-pattern-regex` | The `pattern<>` Compile-Time Regex Engine |
| 12 | `ch12-astnode` | `ASTNode`: The Value-Type Tree |
| 13 | `ch13-leaf-and-node` | Building Trees: `leaf<>` and `node<>` |
| 14 | `ch14-traversal` | Traversal, Dumping, and Tree Utilities |
| 15 | `ch15-rewrite` | The Rewrite Feature: Rules and Predicates |
| 16 | `ch16-rewrite-sets` | Rewrite Sets and Fixpoint Optimization |
| 17 | `ch17-expression-templates` | Expression Templates: Lazy Trees and Fused Evaluation |
| 18 | `ch18-custom-literals` | Custom Literals: Registering Suffixes |
| 19 | `ch19-memoization-and-lazy` | Memoization and Lazy Values |
| 20 | `ch20-maybe-and-result` | `Maybe` and `Result`: Monadic Error Handling |
| 21 | `ch21-parser-core` | The Parser Core: `ParsecInput`, `ExpectedResult`, `Parser` |
| 22 | `ch22-primitive-parsers` | Primitive Parsers and Combinators |
| 23 | `ch23-parser-diagnostics` | Diagnostics, Semantic Actions, and `run_parser` |
| 24 | `ch24-task-pipeline` | Task Pipelines: `Task`, `TaskChain`, `TaskState` |
| 25 | `ch25-peg-definition` | The PEG Definition Grammar: Rules and Channels |
| 26 | `ch26-peg-combinators` | PEG Combinators and the `PEGMatcher` |
| 27 | `ch27-peg-semantic-values` | PEG Semantic Values and AST Construction |
| 28 | `ch28-sexpressions` | S-Expressions: Reader, Printer, and Rule Notation |
| 29 | `ch29-egraphs` | E-Graphs and Equality Saturation |
| 30 | `ch30-codecs-and-io` | User Codecs and Abstract Byte I/O |
| 31 | `ch31-visitors` | AST-Derived Visitors |
| 32 | `ch32-static-regex` | Static Byte Regular Expressions and Binary Streams |
| 33 | `ch33-cfg-earley` | Context-Free Grammars and Earley Parsing |
| 34 | `ch34-preprocessor` | The Preprocessor and Optional Lua |
| 35 | `ch35-directed-graphs` | Directed Graphs: Structure, Traversal, and DOT |
| 36 | `ch36-terms-and-reduction` | First-Order Terms, Unification, and Bounded Reduction |
| 37 | `ch37-d-toolkit` | DSLtk.d: Mixins, CTFE, Native Reflection |
| 38 | `ch38-case-units` | Case Study I: A Units and Physics DSL |
| 39 | `ch39-case-query` | Case Study II: A SQL-Style Query DSL |
| 40 | `ch40-case-config` | Case Study III: A Configuration DSL Built with PEG |
| 41 | `ch41-case-saturation` | Case Study IV: An Equality-Saturation Optimizer |
| 42 | `ch42-case-binary` | Case Study V: A Binary Protocol Toolchain |

---

## Appendix A — Known divergences from the older `DSLtk/manual/`

The following are the most common mistakes; the **shipped header** is
authoritative.

### `dsl::pattern<S>` (line 6175)
* The only entry point is `static constexpr bool matches (std::string_view)`.
  There is **no** `match`, `search`, `full_match`, or `is_match`.
* Matching is **anchored at both ends** — the whole input must be consumed.
* Supported: literals, `.`, `[a-z]` ranges, `*`, `+`.
* **Not** supported: `?` quantifier (treated as a literal `?`),
  `[^...]` negation (`^` is a literal class member), `|` alternation,
  `\d`/`\w` escapes, `$` anchor (a literal `$`), and groups.
* The `+` branch is **greedy and does not backtrack**: `a+ab` does not match
  `"aab"`. The `*` branch does backtrack.

### `dsl::Maybe<T>` (line 7451)
Present: `has_value()`, `as_optional()`, `map`, `flat_map`, `filter`,
`or_else`. **Absent:** `some`, `none`, `map_or`, `and_then`, `value_or`,
`operator bool`, `operator*`, `operator->`.
The free helpers `map`/`flat_map`/`filter`/`or_else` (7511–7541) work with
`std::optional`, **not** with `dsl::Maybe`.

### `dsl::Result<T,E>` (line 7543)
Present: `from_ok`, `from_err`, `is_ok`, `is_err`, `unwrap`, `unwrap_or`,
`map`, `map_err`, `and_then`. **Absent:** `ok()`, `err()`, `value()`,
`error()`, `expect()`, `value_or()`.
`T` and `E` must differ for normal result queries. Indexed factories can
construct `Result<T,T>`, but type-based variant inspection fails to instantiate.
`and_then` requires the callable's return type to have a static `from_err`.

### `dsl::ASTNode` (line 6525)
* Storage is **parallel members** (`tag_`, `value_`, `children_`, `is_leaf_`),
  **not** a `std::variant`.
* Accessors: `tag()`, `is_leaf()`, `value()`, `children()` (const and
  mutable), `child(i)` (throws `std::out_of_range`), `size()`, `dump()`,
  `operator==`. There is **no** `child_at`, no iterators, no
  explicitly declared `operator!=`, no `operator<<`. C++20 rewrites `a != b`
  using the Boolean `operator==`, so that spelling works.

### Parser combinators (lines 7664–7983)
The complete set is: `parser`, `operator*`, `operator&`, `operator|`,
`optional`, `try_parse`, `labeled`, `ch`, `satisfy`, `merge_error`,
`fail_expected`, `run_parser`, `load_parse_input_file`.
There is **no** `dsl::digit`, `dsl::letter`, `dsl::string`, `seq`, `choice`,
`many`, `some`, `preceded`, `followed`, `between`, `sep_by`, `map_parser`.
`ParsecInput` has only `source`, `pos`, `peek()`, `eof()`, `consume()`,
`get_span(start)`.

### PEG subsystem (lines 8123–9085)
* Rules are **compile-time pattern strings** in a regex subset, not
  `"name <- expr"` productions. There is no alternation, grouping, or
  recursion inside a rule pattern.
* `PEGDefinition` exposes `add_rule`, `rules()`, `parse()`, `parent`.
  There is no `rule()`, `channels()`, or `matcher()` method.
* Combinators: `peg_seq`, `peg_choice`, `peg_opt`, `peg_many`, `peg_many1`,
  `peg_and`, `peg_not`, `peg_rule`, `peg_channel`. There is no
  `peg_optional`, `peg_token`, or `peg_literal`.

### S-expressions (lines 9140–9356)
* `SExpr::atom(text, quoted)` and `SExpr::list(items)` are the factories;
  `is_atom` is a **data member**; the printer is `dump()`.
* The `'` quote form is **not supported**. Only `"…"` and automatic
  escalation (empty text or a byte `<= ' '`, `(`, `)`, `"`, `;`, `\`).

### E-graphs (lines 9358–9854)
* `EGraph::unite`, `EGraph::matches`, `EGraph::extract`, `EGraph::extract_ast`.
  There is no `union`, `match`, `lookup`, or `extract_min`.
* Rule syntax: `(rewrite name lhs rhs)`, `(= lhs rhs)`, `(=> lhs rhs)`.

### Visitors (lines 9963–10118)
* Factories are `on<Tag>(fn)` and `ast_schema<Tag>(...)`. There is no
  `visitor_case` and no `schema`.
* The traversal method is `walk`. There is no `visit_all`.
* Exhaustiveness is a `static_assert` over `visitor_detail::covered`.

### I/O (lines 9895–9958)
* `IO::read_exact` and `IO::write_all`; the position accessor is `tell()`.
  There is no `read_all` and no `position()`.

### Static regex (lines 10363–10439)
* `StaticRegex<S>::matches` (whole string), `::prefix` (longest prefix),
  `::search` (leftmost-longest). There is no `match_whole`/`match_prefix`.

### Graph and term subsystems (lines 11405–12024)
* `DirectedGraph`, `Term`, `TermRewriteSystem`, `TermGraph` are free-standing
  types. `Graphs`, `Terms`, `TermRewriting`, `GraphReduction` are **empty**
  marker feature tags.
* `TermRewriteSystem::normalize(term, max_steps = 1000, strategy)`.
* `TermGraph::reduce(root, system, max_steps = 1000, strategy)`.
