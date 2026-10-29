# Chapter 15: Adaptive Lookahead in the Parser Pipeline

## 15.1 Overview

By default the GLR engine pursues every LR action in a conflict cell in
parallel (forking). That is complete but can be wasteful when a short
glance at the upcoming input already rules alternatives out. Adaptive
lookahead is the optional ATN-driven filter that sits between table
lookup and forking: at each conflict cell it scans a bounded window of
following terminals and removes provably non-viable actions before any
stack is forked. When the window does not decide, every action is still
pursued, so enabling the filter can only remove work, never derivations.

This chapter covers enabling and tuning the filter, where it sits in
the pipeline, the exact filtering rules and their soundness argument,
custom ATNs, statistics, and interaction with the rest of the system.

## 15.2 Enabling adaptive lookahead

```c
#include <glr/parser.h>

glr_parser_t *parser = glr_parser_create(grammar);
glr_parser_set_adaptive_lookahead(parser, true);      /* off by default */
glr_parser_set_adaptive_lookahead_depth(parser, 4);   /* optional */
```

Depth counts the conflict token plus following tokens (minimum 1).
Zero selects the default (`GLR_ATN_LOOKAHEAD_DEFAULT_DEPTH`, 8); values
above `GLR_ATN_LOOKAHEAD_MAX_DEPTH` (64) are clamped. Depth 1 still
filters REDUCE candidates by FOLLOW on the conflict token itself; depth
2 or more additionally checks SHIFT continuations against the second
token when lexing ahead is deterministic (15.4). Introspection is
null-safe:

```c
bool on = glr_parser_get_adaptive_lookahead(parser);
size_t depth = glr_parser_get_adaptive_lookahead_depth(parser);
```

The setting survives `glr_parser_reset()` and applies to `glr_parse()`
as well as resumed parses via `glr_parser_parse_from()`.

## 15.3 Where the filter runs

The main loop resolves each scheduled configuration in order:

```text
table lookup (state, lookahead)
  -> adaptive filter (only when the cell holds > 1 action)
  -> exactly one survivor? run it and continue
  -> otherwise fork all survivors, consulting user disambiguators
```

Consequences worth internalizing:

- Non-conflict cells are untouched; deterministic grammars parse
  exactly as before, with no ATN or FOLLOW cost beyond a flag test.
- The filter runs *before* user disambiguation hooks. A cell the filter
  narrows to one action never reaches the hooks; a cell it leaves alone
  behaves exactly as without the filter.
- An empty survivor set keeps every action. The filter prunes only what
  it can prove dead; a total prune is treated as "no information" rather
  than a syntax error.

## 15.4 Window scanning and filtering rules

For a conflict on lookahead token `w0`, the engine scans ahead up to
`depth - 1` further terminal ids using the grammar's own terminal
matching (literal names and scannerless patterns alike), skipping the
configured trivia literal with the same rule the tokenizer uses. The
scan stops at end-of-input, on unscannable bytes, or on lexical
ambiguity: when several terminals match at one offset, the GLR engine
explores each as a parallel alternative, so no deterministic window
extends past that point. Windows are cached per byte offset within a
parse, so many conflicts sharing a position share one scan.

The rules, given window `[w0, w1, ...]` (`-1` for end-of-input):

- **ACCEPT** survives on end-of-input only.
- **SHIFT** always survives its own lookahead (the table guarantees an
  action there). With a deterministic deeper window it additionally
  needs an action in the shift target on `w1`. Shifting into a state
  that cannot consume the forced next token is dead, so the shift is
  dropped.
- **REDUCE** of `H -> alpha` survives exactly when `w0` is in
  `FOLLOW(H)`. No accepting continuation can place anything else after
  the reduced head, so this check is exact.

Two limitations are load-bearing. The reader (UTF-16) path yields one
token at a time, so windows there hold only `w0` and depth behaves as 1.
And the SHIFT continuation check requires deterministic lexing ahead;
under lexical ambiguity only the depth-1 rules apply. Both fall back to
forking rather than guessing.

## 15.5 ATN lifecycle on the parser

The first adaptive parse compiles an ATN from the grammar and a
FIRST/FOLLOW summary, attaches both to the parser, and reuses them
afterwards. When the grammar fingerprint changes (symbols or
productions edited), the summary and any automatically built ATN are
rebuilt; the generated parse table follows the same invalidation
scheme, so the three stay in step.

A custom ATN overrides the automatic one:

```c
glr_atn_t *custom = glr_atn_from_grammar(grammar);
glr_parser_set_atn(parser, custom, true);  /* parser owns custom now */
glr_atn_t *atn = glr_parser_require_atn(parser); /* build if missing */
glr_parser_set_atn(parser, NULL, false);   /* detach, back to auto-build */
```

An attached ATN is reused as-is and never rebuilt, so it must have been
compiled from the parser's grammar; mismatched symbol ids silently
mis-filter. Detaching (or destroying the parser) releases owned ATNs.
`glr_parser_require_atn()` validates the grammar and reports
`GLR_PARSE_ERROR_GRAMMAR` or `GLR_PARSE_ERROR_MEMORY` through the
parser error channel on failure.

## 15.6 Observing the filter: statistics

Counters accumulate across parses for the parser's lifetime:

```c
glr_parser_atn_stats_t stats;
glr_parser_get_adaptive_stats(parser, &stats);
printf("seen=%llu decided=%llu pruned=%llu\n",
       stats.conflicts_seen, stats.conflicts_decided, stats.actions_pruned);
```

`conflicts_seen` counts visited conflict cells, `conflicts_decided`
counts cells narrowed to a single action, and `actions_pruned` counts
removed actions. The invariants `decided <= seen` and
`pruned >= decided` always hold (a decision prunes at least one action).
A healthy adaptive parse of a conflict-heavy grammar shows `seen > 0`
with outcomes identical to plain forking; zero `seen` means the grammar
was deterministic and the filter never engaged.

## 15.7 Manual pipelines: the lookahead hook

Parsers that want hook-level control instead of (or in addition to) the
parser flag can register the factory hook from 14.7. It applies the
FOLLOW rejection to REDUCE candidates at action conflicts and resolves
when one candidate survives. Because unresolved cells return
`GLR_DISAMBIG_NO_MATCH`, it composes with precedence, associativity,
and custom hooks by priority ordering.

## 15.8 Interaction with the rest of the system

- **Disambiguators** still decide genuinely ambiguous cells; the filter
  only removes provably dead actions first.
- **Scannerless grammars** scan ahead through patterns, so variable-length
  terminals participate normally; ambiguous match points bound the
  window instead of forcing a choice.
- **Incremental and live parsing** resume through the same main loop,
  so resumed parses filter identically. Statistics keep accumulating.
- **Semantic actions** run after acceptance and only observe surviving
  derivations, so pruning dead actions cannot change evaluated results.

## 15.9 When to enable it

Enable adaptive lookahead when profiles show conflict forking dominating
parse time: expression grammars with layered operators, statement
grammars with prefix-sharing alternatives, and any grammar whose
generated table reports many conflict cells
(`glr_parse_table_conflict_count()`). Leave it off for deterministic
grammars (nothing to prune) and while debugging table construction
(where seeing every fork is the point).

## 15.10 Summary

- Adaptive lookahead is opt-in per parser, with a bounded window depth.
- It filters only conflict cells, before disambiguators, and falls back
  to full forking whenever undecided.
- ACCEPT/SHIFT/REDUCE rules rest on end-of-input, next-state viability,
  and FOLLOW membership respectively.
- The parser owns a cached ATN plus FOLLOW summary, rebuilds them on
  grammar change, and accepts custom ATNs.
- Statistics distinguish visited, decided, and pruned conflict work.
