# Case Study IV: An Equality-Saturation Optimizer

## The problem: retaining useful alternatives during optimization

The fourth case study builds a small equality-saturation optimizer for
symbolic arithmetic. Its input is `(mul (add a 0) 1)`, and its trusted rules
state that adding zero and multiplying by one preserve a value. The expected
minimum-node representative is the atom `a`. This intentionally modest
example makes it possible to inspect the entire equality workflow and verify
its meaning with an independent evaluator.

The objective is not to claim a production compiler backend. There is no
instruction selection, register allocation, floating-point mode, or custom
machine cost model. The program demonstrates rule loading, e-graph insertion,
bounded saturation, equality queries, observation, extraction, and comparison
with directed term rewriting. These are the implemented facilities that a
larger optimizer would need to integrate.

DSLtk provides the S-expression representation and e-graph engine. The
application provides the arithmetic domain, valid rule set, reference
interpreter, and acceptance criteria for a bounded run. The rule reader
checks syntax and binding discipline; it does not prove the arithmetic
identities. The case study makes that semantic premise explicit instead of
treating structurally valid rules as automatically correct.

## The arithmetic domain and its limits

The evaluator recognizes atoms `a`, `0`, and `1`, plus binary `add` and
`mul`. It receives a concrete integer value for `a` and recursively evaluates
the expression. Unknown atoms, unknown operators, and malformed arities
are rejected. The test domain runs from minus twenty through twenty, where
the chosen expression and rules avoid signed integer overflow.

That bounded test domain is important. C++ signed integer arithmetic is not
an unbounded mathematical ring, and arbitrary expressions can overflow.
The two identities used here are valid for the exercised values and ordinary
defined integer operations. A general optimizer should specify whether it
models mathematical integers, modular arithmetic, checked arithmetic, or
another domain before selecting laws.

Floating-point arithmetic would require a separate analysis. Reassociation,
zero multiplication, and exceptional values can make familiar algebraic
transformations invalid or observably different. Even apparently simple
identities can interact with signed zero, NaNs, rounding, or evaluation
effects. This example avoids those complications by stating a narrow
symbolic integer model and testing within a defined range.

## Rule loading and insertion

The program loads two named rules using `sexpr_rules`. The left-side variable
`?x` appears on the right, so the binding discipline is satisfied. The operator
heads are literal atoms, and each rule has a recognizable name for observers.
Names do not determine matching; the S-expression patterns do.

The input expression is parsed once and inserted into a fresh `EGraph`.
The comparison atom `a` is also inserted, producing a class handle used
later in an equality check. Inserting it does not assert equality with the
whole input. The rules and saturation process must establish that relation.
This makes the later equivalence assertion meaningful.

| Artifact | Meaning | Evidence checked |
|---|---|---|
| Parsed rules | Trusted equalities with bound variables | Rule reader accepts them |
| Input class | All represented alternatives for the input | Stable handle used throughout |
| Atom class | Comparison target | Equivalent only after saturation |
| Saturation report | Operational stopping reason | Must report saturation |
| Extracted expression | Minimum-node representative | Must equal bare `a` |
| Reference evaluator | Application semantics | Original and extracted values agree |

## Bounded saturation is part of the interface

The program passes explicit limits: twenty iterations, one thousand retained
nodes during insertion, and one thousand applications. These are deliberately
generous for the tiny example but make the resource policy visible. A caller
should not rely on an undocumented assumption that every rule system reaches
a fixed point quickly.

The report's `saturated` predicate must be true before the program treats the
run as complete for this rule set. Extraction can be meaningful after a
bounded stop as well, but that is a different acceptance policy. The case
study separately constructs a fresh graph and runs with zero allowed
iterations to verify an `IterationLimit` report and an unchanged extracted
input representation.

That test distinguishes configuration from execution. A zero-iteration budget
does not mean an exception occurred or the input was invalid. It means no
saturation passes were permitted. Preserving the report lets an application
tell users whether it found a fixed point, stopped for resource reasons, or
rejected an invalid rule before running.

## Why four applications can represent two useful equalities

The observer increments a counter after every completed rule application.
The program checks that its count equals the report's application count.
It does not interpret that count as the number of distinct new equalities.
The engine can match and apply a rule again after its equality is already
represented, producing no new graph revision.

For the supplied input and rules, the observed run reports two iterations
and four applications. The changing iteration establishes the relevant
equalities. A later iteration can encounter the same matches while making
no graph change, which is how the fixed-point condition is established.
The revision comparison, not absence of callback activity, determines
saturation.

This distinction matters for instrumentation. A progress bar based only on
application count can look busy even when no new information is being added.
A rule-effectiveness report should distinguish attempts, applications, and
new graph revisions where possible. The current observer exposes rule and
match information; it does not provide a unique theorem-event stream.

## Extraction and equality-triggered processing

After saturation, the program checks that the input root and atom class are
equivalent. It then extracts the root's representative and requires exact
S-expression equality with bare `a`. Minimum-node extraction chooses the
single atom rather than retaining the larger addition and multiplication
forms, while those forms remain represented as alternatives in the graph.

The `when_equal` call sets a local flag when the known equality holds.
This checks the immediate callback API. It does not register a persistent
listener, and the flag is not set by some background event after the call.
Understanding that timing is necessary when using the callback to trigger
an AST visitor, code generation, or another application pass.

The extracted representation remains an `SExpr` for the semantic test.
There is no need to convert to `ASTNode` merely because both are tree-shaped.
Keeping the same representation avoids introducing the AST bridge's
headed-list conventions into an evaluator that already understands symbolic
arithmetic S-expressions.

## The complete executable optimizer

Build the implementation with `make build/saturation`. Its source file is
`projects/saturation/main.cpp`. The aggregate examples target also runs it.
The listing below is taken from that same source file.

```{=latex}
\lstinputlisting[style=dslcpp,caption={Case study IV: bounded saturation with equality, extraction, and semantic checks.},label={lst:case-saturation}]{projects/saturation/main.cpp}
```

Its observed output for the supplied snapshot is:

```text
(mul (add a 0) 1) => a
saturated=1 iterations=2 applications=4
```

The second line is an operational measurement of this run, not a promised
constant for every future implementation. A different matching schedule could
establish the same equality with different application accounting. Tests
should insist on the semantic and stopping contracts they require, while
treating incidental scheduling counts as observations unless the library
explicitly guarantees them.

## Independent evaluation strengthens the test

For every integer `a` from minus twenty to twenty, the program evaluates both
the original and extracted expressions and requires equal results. This is
an independent interpretation of the syntax, not a second call to the rewrite
engine. It can catch a mistaken rule right side or an extraction/conversion
error that a smaller-tree assertion would miss.

The finite sweep is not a mathematical proof for all integers. Its purpose
is to provide meaningful regression evidence within the stated test domain.
For these particular identities, a simple algebraic argument also establishes
the intended law under defined arithmetic. For a larger rule set, combine
domain reasoning, representative examples, randomized bounded tests, and
any formal checks appropriate to the application's requirements.

An evaluator also forces hidden assumptions into code. It must decide arity,
operator names, atom interpretation, and error behavior. Those decisions
become a useful executable specification for the optimizer. Without them,
the phrase equivalent expression can remain too vague to test.

## Comparing directed rewriting

The program constructs a `TermRewriteSystem` with the same two laws and
normalizes the corresponding first-order term. It requires normal form and
the expected constant-like term `a`. This comparison illustrates that the
small example does not need equality saturation to find its answer. A
directed reducer is sufficient for these monotonically simplifying laws.

Equality saturation becomes more useful when laws expose alternatives whose
benefits depend on later transformations. Distribution and factoring, for
example, can move between larger and smaller shapes in ways that make a
single greedy rewrite order difficult to choose. Such rules also enlarge
the search space substantially and require stronger resource discipline.
The compact demonstration establishes the mechanics before introducing those
trade-offs.

The two engines report different units of work. Directed normalization counts
selected redex replacements in one term. Saturation counts applications over
an equality graph and can retain many alternatives. Comparing their raw
counts as if they measured the same operation would be misleading. Compare
semantic results, retained information, and resource behavior instead.

## Extending the optimizer responsibly

Adding constant folding requires a way to recognize and compute literal
values under a precisely specified arithmetic domain. Adding a cost model
for machine execution requires more than the current minimum-node extractor.
Adding type-sensitive laws requires type information or guards that the
existing plain S-expression rule format does not automatically infer.
Each extension should state which new invariant it introduces.

Rule provenance is also useful. A production tool can record where each
trusted rule came from, which domain it assumes, and which regression cases
exercise it. The rule name is a natural reporting hook, but names alone do
not supply proofs. Keep explanatory metadata outside the minimal rule object
when necessary rather than overloading operator spellings with hidden policy.

Finally, preserve partial-result status at the application boundary. A tool
that reaches a node limit may still produce a semantically equivalent
representative, but it should not claim saturation. A tool that extracts a
minimum-node tree should not claim globally optimal machine code. The case
study's strength is the precision of its claims: trusted equalities were
applied under explicit bounds, a fixed point was reported, and the extracted
expression agreed with the independent evaluator over the tested domain.

## Following the equivalence classes through the example

Insertion creates classes for the atoms and compound expressions encountered
in the input. The addition node refers to the classes of `a` and zero; the
multiplication node refers to the addition's class and the class of one.
The add-zero match binds `?x` to the class containing `a`, then merges the
addition's class with that class. The graph has learned an equality while
retaining representations for both the atom and the addition.

The multiplication-by-one rule likewise merges its matched root with the
class bound to `?x`. After rebuilding canonical child references, these
equalities connect the original root to `a`. The stored class handle used
by the application remains useful through the graph's canonicalization
operations; callers ask the graph about equivalence instead of assuming a
handle's numeric value is a permanent canonical representative.

This trace also explains why a smaller extracted tree need not mean that
the graph itself became smaller. Extraction chooses one representative from
retained alternatives. The graph may keep more nodes than the original tree
because it records information about multiple equal forms. A memory budget
therefore cannot be inferred from the size of the extracted result. Track
the retained graph and the selected output as different artifacts.

Congruence is the additional relation that makes the structure an e-graph
rather than a bag of independent rewrite results. If corresponding children
become equivalent, compound nodes with the same operator and equivalent
children must be recognized as equivalent. Rebuilding restores that invariant
after merges. The application need not manually propagate every child
equality to every parent, but it must use the engine's supported operations
so the invariant can be maintained.

## Equality laws need a domain record

A useful rule catalogue records more than pattern text. For add-zero, the
record should identify the operator's semantics, the literal zero's type,
and whether evaluation is pure. For multiplication-by-one, it should state
which arithmetic domain is intended. The current program fixes these facts
through a tiny evaluator. A compiler with several numeric types needs a
representation or rule organization that keeps them from being mixed.

Operator spelling alone is insufficient. An `add` node could represent
integer addition, floating-point addition, matrix addition, string concatenation,
or a user callback. A structurally matching rule can be syntactically valid
and semantically wrong for some of those interpretations. Likewise, a tree
that represents computation with side effects cannot generally discard or
duplicate operands merely because an algebraic identity holds for returned
values. Rule validity is always relative to the language being optimized.

When a law has a side condition, decide how that condition is represented
before encoding the law. A nonzero divisor, matching matrix dimensions, or
an overflow precondition may need analysis information that a plain pattern
does not carry. Encoding an unconditional rewrite and hoping that callers
only provide suitable inputs hides the essential premise. A small supported
rule set with explicit premises is more useful than a broad set of
unqualified textbook identities.

## Resource experiments that answer distinct questions

The zero-iteration test checks the stopping interface before any rule work.
A second useful experiment would use a very small node budget and a rule
that introduces a larger expression. That tests partial-result behavior
under growth. An application-count limit tests a different resource: even
repeated no-change matches can consume work. None of these tests substitutes
for the semantic evaluator, because stopping correctly does not establish
that a rule was sound.

Instrumentation should also record elapsed time separately from reported
applications. Matching a large repeated pattern can be more expensive than
matching a leaf identity. Two runs with equal application counts can have
very different costs. If a tool exposes progress to users, it should present
the counters as observations rather than as a percentage of an unknown
total search space. Equality saturation has no general advance promise of
how much work a fixed point will require.

Finally, preserve the original expression alongside a bounded optimization
result when that helps auditing. A report can show the stopping reason,
chosen representative, extraction cost, and the trusted rule set used. This
does not create a proof certificate, but it makes a run reproducible and
its claims inspectable. The case study already retains enough information
to compare the original and extracted meanings; a larger application can
extend that record without changing the basic separation between equality,
search resources, and output selection.


## Implementation map and review

The following diagram summarizes the chapter's central dependency or state
transition. Arrows denote logical dependence, not elapsed time or measured
performance. The contracts below separate what the library provides from the
conditions that the application must establish.

```{=latex}
\begin{figure}[htbp]
\centering
\begin{tikzpicture}
\node[dslsmall] (n0) {Expression};
\node[dslsmall, right=5mm of n0] (n1) {Bounded saturation};
\draw[dsledge] (n0) -- (n1);
\node[dslsmall, right=5mm of n1] (n2) {Extraction};
\draw[dsledge] (n1) -- (n2);
\node[dslsmall, right=5mm of n2] (n3) {Semantic check};
\draw[dsledge] (n2) -- (n3);
\end{tikzpicture}
\caption{Case study IV distinguishes equalities, budgets, and semantic evidence.}
\label{fig:atlas-41}
\end{figure}
\begin{table}[htbp]
\centering\small
\begin{tabular}{L{0.28\linewidth}L{0.61\linewidth}}
\toprule Contract & Interpretation \\
\midrule
Rules & Application laws must be valid in the chosen arithmetic domain. \\
Limits & A bounded stop is not a claim of saturation. \\
Observer & Application counts are not counts of distinct equalities. \\
\bottomrule
\end{tabular}
\caption{Implementation contracts for this chapter.}
\label{tab:atlas-41}
\end{table}
\begin{pseudocode}[Operational summary]
\textbf{1.} Parse and validate rewrite rules.\\
\textbf{2.} Insert the input expression and comparison atom.\\
\textbf{3.} Saturate under explicit limits.\\
\textbf{4.} Extract a finite minimum-node representative.\\
\textbf{5.} Compare both expressions under the reference evaluator.
\end{pseudocode}
```

### Review questions

1. Why is a finite input sweep evidence rather than a proof?

2. How would floating-point arithmetic invalidate familiar algebraic rules?
