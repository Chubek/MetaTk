# Chapter 14: ATN Representation and Simulation

## 14.1 Overview

This chapter covers the Augmented Transition Network (ATN): the
epsilon-NFA intermediate that LibGLR compiles from a grammar's
productions. The ATN is deliberately independent of the LR parse table,
so a front end can use it for production matching, lookahead analysis,
diagnostics, or code generation while handing the same grammar to the
ordinary GLR parser. Chapter 15 puts it to work as adaptive lookahead
inside the parser pipeline; Chapter 16 covers the C++ façade.

The public API lives in `include/glr/atn.h` with the implementation in
`src/glr/atn.c` (construction and NFA simulation) and
`src/glr/atn_lookahead.c` (FIRST/FOLLOW analysis, prediction, and the
conflict filter).

## 14.2 What the ATN encodes

Compiling a grammar with `glr_atn_from_grammar()` produces one linear
path of symbol transitions per production:

```text
start --eps--> rule(X) --eps--> [X -> a b entry] --a--> o --b--> o (accept p1)
                       \--eps--> [X -> a c entry] --a--> o --c--> o (accept p2)
```

Each nonterminal owns a *rule entry* state that epsilon-branches to the
alternative entries of its productions. Every production path is
independently accepting and tagged with its production id, which makes
the ATN useful for per-alternative reasoning (prediction) as well as
whole-grammar matching. State zero is the start state; the start state
epsilon-branches to every rule entry.

Two entry queries expose this structure:

```c
#include <glr/atn.h>

uint32_t rule = glr_atn_rule_start(atn, x_id);       /* entry for X */
uint32_t entry = glr_atn_production_start(atn, prod); /* entry for one alternative */
```

Both return `UINT32_MAX` for unknown ids or NULL ATNs. Rule entries
answer "what can X derive?"; production entries answer "what does this
specific alternative match?", which is what adaptive prediction simulates
from (see 14.6).

## 14.3 Building an ATN from a grammar

```c
glr_atn_t *atn = glr_atn_from_grammar(grammar);
if (atn == NULL) {
    /* grammar failed structural validation or allocation failed */
}
```

Compilation validates the grammar first: a start symbol must be set,
every production head must be a nonterminal owned by the grammar, and
every body symbol must belong to the grammar. Compilation fails (returns
NULL) otherwise. The ATN borrows nothing from the grammar after
construction; grammars and ATNs have independent lifetimes, and the
caller destroys the ATN with `glr_atn_destroy()` (NULL is accepted).

Because symbol transitions carry grammar symbol ids, an ATN compiled
from one grammar must never be applied to another grammar's ids. The
parser pipeline handles this by rebuilding its automatic ATN whenever
the grammar fingerprint changes (see 15.5); a manually attached ATN is
the caller's responsibility.

## 14.4 Manual construction and inspection

Hand-built ATNs are useful for tests, diagnostics, and custom front
ends:

```c
glr_atn_t *atn = glr_atn_create(); /* state 0 is the start state */
uint32_t b = glr_atn_add_state(atn);
uint32_t c = glr_atn_add_state(atn);
glr_atn_add_epsilon(atn, glr_atn_start_state(atn), b);
glr_atn_add_symbol(atn, b, c, terminal_id);
glr_atn_set_accepting(atn, c, true, production_id);
```

`glr_atn_add_symbol()` rejects negative symbol ids. `glr_atn_state()`
returns a borrowed state descriptor (transitions, accepting flag,
production id), and `glr_atn_state_count()` reports the size. State sets
(`glr_atn_state_set_t`) are plain id collections with create/destroy/
clear/add/contains/count/at operations used to thread simulations.

## 14.5 Simulation: closure, stepping, and matching

The simulation core is three operations:

```c
glr_atn_state_set_t *set = glr_atn_state_set_create();
glr_atn_state_set_add(set, start);
glr_atn_epsilon_closure(atn, set);            /* in-place closure */
glr_atn_step(atn, set, symbol_id, next);      /* consume one symbol */
bool done = glr_atn_state_set_accepting(atn, set);
```

`glr_atn_step()` supports aliasing the input and output sets. Whole
sequences match with `glr_atn_match()` (from the start state) or
`glr_atn_match_from()` (from an explicit state such as a rule entry),
returning 1 for a match, 0 for rejection, and -1 on invalid input. This
is NFA matching over symbol ids; it does not perform CFG recursion.

For lookahead work, the window-oriented helpers are more convenient than
exact matching. A *window* is a short array of terminal ids where -1
(`GLR_ATN_LOOKAHEAD_EOF`) marks end-of-input:

```c
int tokens[] = { a_id, b_id };
int viable = glr_atn_is_prefix_viable(atn, rule, tokens, 2); /* 1/0/-1 */
long kept = glr_atn_viable_prefix_length(atn, rule, tokens, 2); /* 0..2 */
```

End-of-input terminates the viable span without being counted: a window
ending at EOF is viable exactly when its terminal prefix is. Negative
ids other than -1 are invalid. Nonterminal-labelled edges are traversed
only for the exact id given and never expanded; recursive prediction
over nonterminals is layered on top (14.6).

## 14.6 FIRST, FOLLOW, and production prediction

`glr_atn_follow_compute()` derives the standard nullable/FIRST/FOLLOW
fixpoint for a grammar. The summary carries one extra end-of-input
column at index `symbol_count`, matching the parse table's EOF column
convention:

```c
glr_atn_follow_t *look = glr_atn_follow_compute(grammar);
bool follows = glr_atn_follow_contains(look, head_id, token_id); /* -1 for EOF */
bool starts = glr_atn_first_contains(look, symbol_id, token_id);
glr_atn_follow_destroy(look);
```

Both queries are false on any invalid input rather than trapping, which
keeps the parser's hot path branch-simple.

Prediction selects among one nonterminal's alternatives:

```c
int winner = -1;
int rc = glr_atn_predict_production(atn, grammar, x_id,
                                    window, window_count, &winner);
if (rc == 1) {
    /* winner holds the predicted production id */
} else if (rc == 0) {
    /* undecided: the window does not distinguish the alternatives */
} else {
    /* invalid input */
}
```

Every production headed by the nonterminal is scored by walking its ATN
path from its production entry: terminal RHS symbols must equal the
window token, and nonterminal RHS symbols consume one window token when
that token is in the nonterminal's FIRST set. The unique longest-prefix
winner is reported; ties (including the shared-prefix case, where every
alternative scores equally) report undecided instead of guessing. An
undecided answer is a normal outcome, not an error: the parser falls
back to full GLR forking.

## 14.7 Lookahead disambiguation hook

For pipelines that prefer hooks over parser flags,
`glr_atn_lookahead_hook_create()` builds a self-contained
disambiguation hook. The hook owns a private ATN compiled from the
grammar plus its FOLLOW summary, rejects REDUCE candidates whose head
cannot be followed by the conflict's lookahead symbol, and reports a
winner only when exactly one candidate survives. Otherwise it returns
`GLR_DISAMBIG_NO_MATCH` so later hooks still run:

```c
struct glr_disambig_hook *hook =
    glr_atn_lookahead_hook_create("atn-lookahead", 10, grammar, 8);
glr_parser_add_disambiguator(parser, hook); /* parser owns hook on success */
```

Include both `<glr/atn.h>` and `<glr/disambiguate.h>` to use the
factory. Ownership follows the disambiguation API: a successful
registration transfers ownership to the parser, otherwise the caller
destroys the hook.

## 14.8 Summary

- The ATN is an epsilon-NFA over production alternatives, independent
  of the LR table.
- Rule entries branch over a nonterminal's alternatives; production
  entries isolate one alternative for prediction.
- Simulation is closure/step/match over state sets; windows use
  terminal ids with -1 for end-of-input.
- FIRST/FOLLOW summaries and `glr_atn_predict_production()` turn the
  ATN into a bounded, honest predictor that reports undecided rather
  than guessing.
