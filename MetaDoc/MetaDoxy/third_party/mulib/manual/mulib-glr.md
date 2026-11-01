# mulib-glr — generalized LR CFG parsing

`include/mulib-glr.h` compiles a context-free grammar into canonical **LR(0)
item sets**, then parses with a graph-structured stack (`mulib-gss`) and an
n-ary shared packed parse forest (`mulib-sppf`). All shift/reduce and
reduce/reduce conflicts are retained. An ambiguous grammar produces an
ambiguous forest; no branch is chosen merely because it was inserted first.

## Supported scope

* Arbitrary finite CFG productions with explicitly identified nonterminals and
  terminals. Empty RHS means epsilon. A nonterminal need not have a production.
* Inclusive `uint32_t` terminal ranges. Each terminal consumes exactly **one
  token**; zero and `UINT32_MAX` are ordinary values. Overlapping terminal
  ranges branch independently, retaining their distinct symbol IDs.
* Nullable symbols, direct and indirect left recursion, cyclic grammars, and
  finite graphs representing infinite ambiguity. Unproductive cycles reject
  rather than loop forever. Work/entry limits can instead stop expensive cases.
* Whole-input recognition and all derivations rooted at the designated start
  nonterminal. Duplicate production texts with different production IDs count
  as distinct choices.

There is no textual grammar reader, lexer/UTF-8 decoder, precedence or
associativity directive, semantic action, recovery, incremental operation,
probability/ranking system, or implicit ambiguity resolution. Callers supply
token arrays. Forests are not binarized, and this is **not** an optimized
RNGLR/BRNGLR implementation or a claimed cubic-time general parser.

## Actual algorithm

Compilation encodes each production's dot positions as ATN states, with
symbol-labelled transitions advancing the dot and epsilon transitions
predicting productions of an expected nonterminal. Per-symbol hub states share
these prediction edges. Subset construction using ATN epsilon closure produces
the canonical LR(0) transition machine. Complete ordinary items provide reduce
actions; the complete augmented start item marks acceptance. The ATN consumes
**grammar symbol IDs during compilation**, not the eventual input token values.

At input position `k`, a GSS node is shared by `(LR state, k)`. An edge points
from a top to a predecessor and is labelled by an SPPF symbol node. For each
completed production, the parser iteratively enumerates every matching
predecessor path of exactly the RHS length. Each path contributes a packed
alternative and a goto edge. Reductions are repeated until no new GSS edge is
added. A new alternative beneath an existing symbol node needs no new stack
edge: all users already reference that shared symbol.

Only after this fixed point does the parser shift the next token along **all**
matching terminal transitions. At EOF, acceptance requires an accepting LR
state with a start-symbol edge spanning `[0, length)` back to the initial node.
There is no deterministic fallback, lookahead pruning, or artificial EOF token.

Deduplication bounds same-position saturation, even when nullable reductions
make GSS/SPPF cycles. Enumeration has a fixed path length and uses explicit
frames, not recursion. These choices make cyclic cases safe within limits;
they do **not** make every grammar cheap. Long RHSs and heavily ambiguous
nullable grammars can require exponentially many path combinations. Forest
alternatives contain all RHS children directly, not binary intermediate nodes.

## Integration and ownership

ISO C11; declaration-only use is C++17-compatible. Define every transitive
implementation switch before any MuLIB include in exactly one translation unit:

```c
#define MULIB_IMPL_VEC
#define MULIB_IMPL_BITSET
#define MULIB_IMPL_ATN
#define MULIB_IMPL_GSS
#define MULIB_IMPL_SPPF
#define MULIB_IMPL_GLR
#include "mulib-glr.h"
```

No external libraries. None of these headers defines another module's switch.
The grammar owns copies of its symbols, productions, RHSs, and compiled machine.
Each parse result owns its forest and GSS independently. It neither retains the
input tokens nor borrows grammar storage. Symbol/production IDs in a result
still refer to that grammar's stable IDs; retain that mapping if needed after
freeing the grammar. Terminal semantic values must be read from retained input
tokens using the leaf span: the forest does not store token payloads.

A compiled grammar is immutable during `parse` and may be shared by concurrent
parses with separate results. Grammar edits/compilation and mutations of the
same result object are not internally synchronized.

## API

All IDs are zero-based `size_t`; `MULIB_GLR_NONE` is `SIZE_MAX`.

| Function | Contract |
| --- | --- |
| `mulib_glr_limits_default(&limits)` | Fill default values; NULL is ignored. |
| `mulib_glr_create(&limits)` | Owning empty grammar, or NULL on allocation failure; NULL limits selects defaults. Each zero field also selects its default. |
| `mulib_glr_destroy(g)` | Free grammar and machine; NULL is safe. Does not free results. |
| `mulib_glr_nonterminal(g, &symbol)` | Append a nonterminal. |
| `mulib_glr_terminal(g, lower, upper, &symbol)` | Append a terminal; `lower <= upper` is required. |
| `mulib_glr_production(g, lhs, rhs, length, &production)` | Append a rule; lhs must be nonterminal and every RHS symbol must already exist. NULL RHS is valid only for length zero. Copies the RHS, including an RHS borrowed from the same grammar. |
| `mulib_glr_symbol(g, id, &record)` | Copy `{kind, lower, upper}`; range fields apply only to terminals. |
| `mulib_glr_rule(g, id, &lhs, &rhs, &length)` | Borrow a read-only RHS pointer, NULL for epsilon. All outputs are required. Next successful production append invalidates this pointer; failed appends do not. |
| `mulib_glr_symbol_count(g)`, `mulib_glr_production_count(g)` | Counts, or zero for NULL. |
| `mulib_glr_state_count(g)` | Number of compiled LR states, zero when uncompiled or NULL. |
| `mulib_glr_compile(g, start)` | Compile using a nonterminal start symbol. Transactional: failure retains any existing compilation. |
| `mulib_glr_result_init(&result)` | Initialize a **fresh** result. Does not free existing storage. |
| `mulib_glr_result_free(&result)` | Destroy forest/GSS and reset fields. NULL is safe. |
| `mulib_glr_parse(g, tokens, length, &result)` | Parse into an initialized, empty result; NULL tokens is valid only for empty input. |
| `mulib_glr_status_string(status)` | Static explanatory text; never NULL. |

Successful symbol/rule edits invalidate and free the previous compilation;
call `compile` again. Failed edits leave logical entries and compilation intact.
All insertion ID outputs are required and remain untouched on failure.
Production IDs remain distinct even for identical lhs/RHS text.

## Results, errors, and forest traversal

| Status | Meaning and result ownership |
| --- | --- |
| `MULIB_GLR_OK` (0) | Accepted; `root` identifies the start-symbol SPPF node and `position == length`. |
| `MULIB_GLR_SYNTAX` (1) | Rejected; `root == NONE`. The result owns a partial forest/GSS for diagnostics. `position` is the first token that no branch could shift, or length if all tokens were consumed but EOF was not acceptable. |
| `MULIB_GLR_INVALID` (-1) | Invalid argument/ID/range, terminal lhs/start, or nonempty/uninitialized result. |
| `MULIB_GLR_NOMEM` (-2) | An allocation failed. |
| `MULIB_GLR_LIMIT` (-3) | Entry/work bound or representable allocation size exceeded. **Not** proof that input is invalid. |
| `MULIB_GLR_UNCOMPILED` (-4) | Compile after the last successful edit before parsing. |

Negative parse errors leave a valid originally empty result empty and release
temporary artifacts. Parsing never implicitly overwrites a nonempty result.
Always call `result_free` after success **or** SYNTAX. Merely zero-initializing a
result is insufficient, since root must be NONE: use `result_init`.

Forests contain reachable and possibly unreachable work from unsuccessful
branches. Traverse **from result.root** for accepted derivations. Symbol nodes
are keyed by grammar symbol and span. For each nonterminal, follow
`first_alternative` / `alternative.next`; retrieve child IDs with
`mulib_sppf_child`. Packed `production` values are grammar production IDs.
Do not recursively traverse cycles without a visited set or explicit limit.

`mulib_sppf_count(result.forest, result.root, cap, max_steps, &count)` gives an
exact finite count, reports INFINITE for productive cyclic ambiguity, or LIMIT
for count saturation/work exhaustion. See `mulib-sppf.md` for the distinction.
A finite cyclic forest can represent infinitely many finite trees, for example
`S -> S | epsilon`; no attempt is made to enumerate an infinite set of trees.

## Exact resource scope

Each zero field of `mulib_glr_limits_t` selects the default:

| Field | Default | Scope |
| --- | ---: | --- |
| `max_symbols` | 1024 | Grammar symbols; also clamped to `UINT32_MAX`. |
| `max_productions` | 4096 | Grammar production records. |
| `max_rhs_symbols` | 65536 | Sum of RHS lengths over all productions. |
| `max_lr_states` | 1024 | Canonical LR subsets per compilation. |
| `max_lr_edges` | 65536 | LR symbol transitions per compilation. |
| `max_gss_nodes` | 65536 | Shared stack nodes per parse. |
| `max_gss_edges` | 262144 | Shared predecessor edges per parse. |
| `max_sppf_nodes` | 65536 | Forest symbol nodes per parse. |
| `max_alternatives` | 262144 | Forest packed choices per parse. |
| `max_children` | 1048576 | Sum of child-ID references in packed choices. |
| `max_tokens` | 65536 | Input length per parse. |
| `max_steps` | 10000000 | Separate compilation and parsing work budgets. |

If `R` is the total RHS length, `P` production count, and `S` symbol count,
the temporary compilation ATN has `R + P + 2 + S` states and at most
`2R + P + 2` edges; LR subset bitsets each have that many bits. Compilation
work includes grammar-item/state visits, ATN subset-operation work, LR candidate
comparisons, and source-state/symbol visits. Parsing work counts stack-node,
transition, reduction, path-enumeration, and acceptance loop visits. The count
operation has its **own** SPPF work budget.

These are logical-entry and coarse algorithm-work limits, **not exact byte or
wall-clock limits**. Bitset operations/copies, allocator overhead, vector growth,
and linear container interning scans are not separately metered. Vectors may
reserve beyond their logical lengths. Compilation can have exponentially many
LR states; parsing explicitly enumerates reduction paths and rescans earlier
stack nodes. Reduce grammar size/RHS length or increase appropriate limits for
larger jobs; a LIMIT result is deliberately not disguised as SYNTAX. All size
arithmetic and allocation failures are checked. C call-stack depth does not
depend on grammar recursion or input length.

## Runnable ambiguous example

`S -> SS | a` has two parses of `aaa`:

```c
#define MULIB_IMPL_VEC
#define MULIB_IMPL_BITSET
#define MULIB_IMPL_ATN
#define MULIB_IMPL_GSS
#define MULIB_IMPL_SPPF
#define MULIB_IMPL_GLR
#include "mulib-glr.h"
#include <assert.h>

int main(void) {
    mulib_glr_t *g = mulib_glr_create(NULL);
    mulib_glr_result_t r;
    size_t s, a, rule, rhs[2], count;
    uint32_t input[] = {'a', 'a', 'a'};
    assert(g);
    assert(!mulib_glr_nonterminal(g, &s));
    assert(!mulib_glr_terminal(g, 'a', 'a', &a));
    rhs[0] = s; rhs[1] = s;
    assert(!mulib_glr_production(g, s, rhs, 2, &rule));
    assert(!mulib_glr_production(g, s, &a, 1, &rule));
    assert(!mulib_glr_compile(g, s));
    mulib_glr_result_init(&r);
    assert(!mulib_glr_parse(g, input, 3, &r));
    assert(!mulib_sppf_count(r.forest, r.root, SIZE_MAX, 0, &count));
    assert(count == 2);
    mulib_glr_result_free(&r);
    mulib_glr_destroy(g);
    return 0;
}
```

`examples/modules/glr.c` also checks invalid input. The dedicated test covers
nullable ambiguity, left recursion, productive/dead cycles, duplicate rules,
overlapping ranges, hard limits, allocation-failure injection, and comparison
against an independent span-relation CFG recognizer.

```sh
cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude examples/modules/glr.c -o ./glr
./glr
cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude tests/test_grammar.c -o ./test_grammar
./test_grammar
```
