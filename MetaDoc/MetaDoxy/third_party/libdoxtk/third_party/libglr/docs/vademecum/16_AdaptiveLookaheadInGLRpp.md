# Chapter 16: Adaptive Lookahead in GLRpp

## 16.1 Overview

GLRpp, the header-only C++20 façade (`GLRpp/GLRpp.hpp`), exposes the
ATN lookahead pipeline through two additions: a move-only `glrpp::Atn`
handle over the C ATN, and adaptive-lookahead controls on
`glrpp::Parser`. Everything else in this chapter assumes the grammar
and parser setup from Chapter 13; only the lookahead pieces are new.

```cpp
#include <GLRpp/GLRpp.hpp>
```

## 16.2 The `glrpp::Atn` handle

`glrpp::Atn` owns a `glr_atn_t*` with the same move-only RAII discipline
as the other GLRpp handles:

```cpp
glrpp::Grammar grammar;
auto s = grammar.nonterminal("S");
auto x = grammar.nonterminal("X");
auto a = grammar.terminal("a");
auto b = grammar.terminal("b");
auto c = grammar.terminal("c");
grammar.add_production(s, {x});
grammar.add_production(x, {a, b});
grammar.add_production(x, {a, c});
grammar.set_start(s);

glrpp::Atn atn = glrpp::Atn::from_grammar(grammar);
```

Inspection covers counts, both entry kinds, matching, and prefix length:

```cpp
size_t states = atn.state_count();
uint32_t start = atn.start_state();
uint32_t rule = atn.rule_start(x.id());
uint32_t entry = atn.production_start(1);
int matched = atn.match({a.id(), b.id()});            // 1
long kept = atn.viable_prefix({a.id(), b.id()}, rule); // 2
```

Windows are `std::vector<int>` of terminal ids; end-of-input is
`GLR_ATN_LOOKAHEAD_EOF` (-1), the same sentinel as the C API.

## 16.3 Predicting alternatives

`predict()` wraps `glr_atn_predict_production()` and reports the
outcome through `dsl::Result`, consistent with the rest of GLRpp:
success carries the winning production id, while ties and errors both
surface as `is_err()` with distinct messages ("lookahead undecided"
versus a failure notice):

```cpp
auto decided = atn.predict(grammar, x.id(), {a.id(), b.id()});
if (decided.is_ok()) {
    int production = decided.unwrap(); // production 1
}
auto tied = atn.predict(grammar, x.id(), {a.id()});
bool undecided = tied.is_err(); // shared one-token prefix
```

Prediction needs the grammar alongside the ATN because alternatives are
enumerated from the grammar while simulation walks ATN paths. Keep both
alive for the call; neither is retained afterwards.

## 16.4 Parser controls

Adaptive parsing is two calls on `glrpp::Parser`:

```cpp
glrpp::Parser parser(grammar);
if (parser.enable_adaptive_lookahead(4).is_err())
    throw std::runtime_error("adaptive setup failed");

auto result = parser.try_parse("ab");
bool on = parser.adaptive_lookahead_enabled();
size_t depth = parser.adaptive_depth();
auto stats = parser.adaptive_stats();
```

`enable_adaptive_lookahead()` defaults to
`GLR_ATN_LOOKAHEAD_DEFAULT_DEPTH` when no depth is given; depth 0 also
selects the default and oversized values clamp to
`GLR_ATN_LOOKAHEAD_MAX_DEPTH`, mirroring the C setters. Failures return
`Status` errors rather than throwing, except `adaptive_stats()`, which
throws only when the parser handle itself is unusable. `disable_adaptive_lookahead()`
restores plain GLR forking; parsing behaviour is otherwise unchanged.

A custom ATN attaches with ownership transfer, and the parser-owned ATN
can be demanded into existence:

```cpp
glrpp::Atn custom = glrpp::Atn::from_grammar(grammar);
parser.attach_atn(std::move(custom)); // parser owns it now
glr_atn_t *borrowed = parser.require_atn(); // throws on failure
```

After `attach_atn()`, the moved-from `Atn` is empty and must not be
used. The borrowed pointer from `require_atn()` follows the C lifetime:
valid while the parser lives and no replacement ATN is attached.

## 16.5 End-to-end example

```cpp
glrpp::Grammar grammar;
auto expr = grammar.nonterminal("Expr");
auto number = grammar.terminal("n");
auto plus = grammar.terminal("+");
auto star = grammar.terminal("*");
grammar.add_production(expr, {expr, plus, expr});
grammar.add_production(expr, {expr, star, expr});
grammar.add_production(expr, {number});
grammar.set_start(expr);

glrpp::Parser parser(grammar);
parser.enable_adaptive_lookahead();

auto parsed = parser.try_parse("n+n*n");
if (parsed.is_err())
    throw std::runtime_error("parse failed");

auto stats = parser.adaptive_stats();
// stats.conflicts_seen > 0 on this ambiguous grammar;
// the forest still holds every surviving derivation.
```

Disabling the filter and re-parsing must produce the same accepted
language; use that as a regression check when tuning depths or custom
ATNs. The statistics struct (`conflicts_seen`, `conflicts_decided`,
`actions_pruned`) is the C `glr_parser_atn_stats_t` by value, so it
stays valid after the call.

## 16.6 Summary

- `glrpp::Atn` wraps ATN construction, entries, matching, prefix
  lengths, and alternative prediction with RAII ownership.
- `glrpp::Parser` gains depth-configured adaptive parsing, stats, and
  custom-ATN attachment with the façade's usual `Result`/exception
  split.
- Adaptive and plain parses accept the same language; statistics show
  what the filter removed.
