# datalog: finite positive rule evaluation

`mulib-datalog.h` computes a least fixed point for positive relational rules over
finite string constants. It supplies a **C term/atom API**, not a text parser or
Prolog interpreter. There are no module or third-party dependencies.

## Integration and example

Define `MULIB_IMPL_DATALOG` once, and include normally elsewhere. Declarations
support C++ consumers; implementation requires C11.

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude examples/modules/datalog.c -o datalog
./datalog
```

The example inserts `edge(alice,bob)` and `edge(bob,carol)`, constructs the rules
below using C records, saturates, and prints all three reachable pairs:

```text
path(X,Y) :- edge(X,Y).
path(X,Y) :- path(X,Z), edge(Z,Y).
```

Here is a single copied ground fact:

```c
mulib_datalog_term_t terms[] = {{"alice", 0}, {"bob", 0}};
mulib_datalog_atom_t edge = {"edge", terms, 2};
mulib_datalog_t *db = mulib_datalog_create(NULL, NULL);
if (db) {
    mulib_datalog_status_t status = mulib_datalog_fact(db, &edge);
    (void)status;
}
mulib_datalog_destroy(db);
```

## Exact model

An atom is a predicate name, arity, and ordered terms. A term with `variable=0`
is a literal string constant; any other value means a named variable. Predicate
names and variable names must be nonempty. Constants may be empty. All names
are case-sensitive NUL-terminated byte strings, not implicitly Unicode-normalized
or numeric. Predicate and arity together identify a relation; `p/1` and `p/2`
are separate relations. Nullary predicates are valid.

Facts must be ground. A rule has one head and a nonempty conjunction of body
atoms. Each head variable must occur somewhere in the body (range restriction).
Body-only variables are allowed. Variables are scoped per rule, identified by
name, and copied into bounded local variable indices. Repeated occurrences
unify with the same constant, including within one atom. `_` is an ordinary
named variable, **not** an anonymous wildcard. Ground rule heads are supported.

Rules are positive and function-free. No negation, disjunction, arithmetic,
aggregation, terms containing functors, side-effect predicates, or ordered
backtracking is advertised. Facts use set semantics; reinserting a duplicate
succeeds without consuming a fact slot. Rules themselves are not deduplicated.

`mulib_datalog_saturate` performs naive fixed-point evaluation. Each round joins
rule bodies against the round's fact snapshot, substitutes the head, and inserts
distinct ground tuples. Rules never invent constants; all constants were copied
when facts/rules were registered. Consequently the universe is finite. A final
unchanged round establishes the fixed point. Cycles, recursion, repeated variables,
and mutual recursion are supported, subject to limits. The implementation favors
small embedded rule sets over indexing/large-data performance.

## Queries and ownership

`mulib_datalog_query` matches one atom against **currently stored** facts; call
`saturate` first when closure is required. Pattern constants match literally and
named variables unify as above. Unknown predicates/constants give zero matches
and do not mutate the symbol table. A ground query has zero or one match.

The callback receives the entire ground tuple, not just variable bindings.
Tuple array and strings are borrowed; copy what you need. Strings stay owned by
the database. Do not mutate/reenter the database from callbacks. A nonzero
callback result returns `MULIB_DATALOG_STOPPED`; the count includes that row.
A NULL callback only counts. Enumeration order is insertion order, not a
relational ordering guarantee. A query limit can return a partial count.

Facts/rules copy every needed name; stack-local inputs may be reused immediately.
Destroy releases all copied data and accepts NULL. Failed fact/rule registration
is atomic, including symbol interning rollback. Failed saturation rolls back
**all facts derived by that call**, preserving earlier facts and rules. Retrying
with the same exhausted limits will fail again; choose budgets at creation.
New facts/rules may be added after successful saturation and saturated again.
No deletion, truth maintenance, persistent serialization, or thread safety.

## Budgets and diagnostics

`mulib_datalog_default_limits()` selects 4096 facts, 256 rules, 4096 interned
symbols, 65536 total symbol bytes including terminators, arity 8, body length 16,
32 variables per rule/query, 1 million work steps, and 256 rounds per saturation.
These are exact maxima; a supplied zero is a real zero. Arity/body/variable maxima
above 8/16/32 are rejected at creation. Fact/rule/symbol tables are allocated once
at creation with multiplication overflow checks. String allocation is incremental
and checked against the aggregate symbol-byte limit. Names of predicates and
constants share the intern table.

Every attempted rule application, candidate body-fact match, and head duplicate
comparison consumes saturation work. A query consumes one step per inspected
fact, resetting the budget each call. Registration does not consume that work
budget; it is bounded in storage by the table/string budgets. Depth of joins is
at most 16, variable bindings at most 32. The round budget includes the final
unchanged round. Tight limits can reject a mathematically finite computation.

Statuses distinguish invalid records, unsafe head variables, resource exhaustion,
allocation failure, and callback stop. Creation optionally reports its failure
status. `mulib_datalog_fact_count` includes base and derived facts.
`tests/test_language_tools.c` exercises recursive closure, repeated-variable
unification, nullary/unknown/empty queries, unsafe rules, duplicates, rollback on
fact/step/round budgets, and allocation failure.
