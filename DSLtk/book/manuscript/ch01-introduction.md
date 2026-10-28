# Introduction: The Problem of Language Construction

## Scope, sources, and the meaning of a language toolkit

This book studies the actual `DSLtk/DSLtk.hpp` supplied with the project. The
secondary subject is `DSLtk/DSLtk.d`, whose different implementation choices
help separate the toolkit's ideas from C++ syntax. A language toolkit is not
itself a language specification. It can construct a syntax tree without knowing
whether `add` means integer arithmetic, vector addition, concatenation, or an
operation that emits machine instructions. Choosing that meaning is the
language designer's responsibility.

The project's existing `DSLtk/manual` provides background and a foundation
for the D reference material. The independently authored technical chapters
follow the implementation, with algorithms, design trade-offs, and worked
applications. Five new case studies
have companion programs under `DSLtkBook/projects`. Their output and assertions
are executable evidence for the behavior discussed. Source excerpts and
application fragments elsewhere explain mechanisms; they are not all
standalone translation units. Negative examples describe rejected programs.
The companion verification report distinguishes tests actually run from claims
made by historical documentation.

This distinction matters because the supplied source has a few observable
defects. In C++, the predicate composition members attempt to construct the
current `Predicate<F>` specialization from a new lambda type. That fails on
ordinary composition. The same-type `Result<T,T>` can be constructed, but its
type-based variant queries cannot be instantiated. The task runner uses exactly
that unsupported specialization. These are specific properties of this
snapshot, not general limitations of predicate algebra or result types. The
book explains their intended algorithms while providing working alternatives
at the application boundary. The library files themselves are the reference
under study; the book does not silently repair them.

The distinction between static and dynamic work is equally important. CRTP
avoids a virtual interface for feature injection. It does not make vector
allocation, string formatting, parser calls through `std::function`, graph
hashing, or recursive copying disappear. A compile-time tag can select a
factory while the factory still allocates an ordinary runtime tree. A
compile-time regex can contain runtime NFA simulation. Good DSL design depends
on recognizing exactly which decisions happen at each stage.

Read Chapters 1--6 for composition and contracts, Chapters 7--20 for the basic
value-level facilities, and Chapters 21--27 for parsing and semantic actions.
Chapters 28--36 connect symbolic interchange, equality reasoning, data
boundaries, graph algorithms, and reduction. Chapter 37 is the consolidated D
comparison. Chapters 38--42 are five complete application narratives: physical
units, queries, configuration, saturation, and binary framing. Each case study
states the part supplied by DSLtk and the part implemented by the application.

## A vocabulary before a grammar

A useful first question is not how to parse a language but what distinctions
its users need to express. A units language distinguishes distance from time;
a query language distinguishes row selection from projection; a binary
protocol distinguishes framing from payload interpretation. If these
distinctions are absent from the representation, a prettier syntax will not
recover them. Conversely, a carefully chosen representation can support both
an embedded C++ interface and an external textual syntax without duplicating
the semantics.

For that reason, the case studies begin with a contract and an executable
model. Their notation is deliberately modest. A small language with explicit
meaning, defined failure behavior, and a testable implementation is a stronger
foundation than a large grammar whose operations remain informal. DSLtk's
chief contribution is making that small foundation inexpensive to assemble.

## Embedded and external languages share a semantic problem

An embedded language expresses domain operations through the host language.
In C++, a quantity can use overloaded division, a query can use factory
functions, and a parser can use operator composition. The compiler already
knows how to tokenize identifiers, check function calls, instantiate templates,
and report many type errors. Reusing those services is a substantial practical
advantage. It also means that the language inherits C++ precedence, lifetime
rules, diagnostics, and the distinction between compile-time and runtime values.

An external language starts with its own text or bytes. It can offer notation
that would be awkward in C++, allow users to edit configuration without
recompilation, and preserve source locations that belong to the domain. In
exchange, its implementation must define recognition, parse errors, validation,
and the relationship between a successful parse and an executable value. A
grammar alone supplies only part of that relationship. Accepting the text
`port=0;` does not establish that zero is a valid listening port.

The same semantic core can support both forms. A query factory and a query
parser can produce the same validated tree. A typed units API and a suffix
parser can construct the same quantity. This is often a better architecture
than making one notation the owner of all meaning. Tests of interpretation
then remain useful when the surface syntax changes, and an additional front
end does not require another implementation of the domain's arithmetic or
evaluation policy.

DSLtk contains facilities for both styles, but it does not prescribe a single
compiler pipeline. An application may use only `pipe`, only `StaticRegex`, or
only `DirectedGraph`. Another may combine preprocessing, tokenization, a tree,
rewriting, and serialization. Treat the feature catalogue as a collection of
possible components. Select a component because its representation and
contract fit a concrete problem, rather than because a language implementation
is expected to contain a stage with that name.

## A first complete program

The following program uses an immediate pipeline. Each stage receives the
preceding value and returns the next. The host selects the `Pipeline` feature
to obtain the static `wrap` convenience member. The actual pipe operator is a
free function associated with the stage type, so this example also introduces
the distinction between composing a host and calling standalone facilities.

```cpp
#include "DSLtk.hpp"
#include <cassert>
#include <iostream>

struct Arithmetic : dsl::DSL<Arithmetic, dsl::Pipeline> {};

int main() {
    auto result = Arithmetic::wrap(5)
        | dsl::pipe([](int x) { return x + 3; })
        | dsl::pipe([](int x) { return x * 2; });
    assert(result == 16);
    std::cout << result << '\n';
}
```

The output is `16`. Evaluation proceeds left to right because the pipe
expressions associate left to right: first add three to five, then double
eight. Nothing here schedules background work or records a task graph. The
callables run as the expression is evaluated. The separate task facility
described later has a different state model even though it also uses the
vertical-bar character in its notation.

Build this program with a C++20 compiler and an include path pointing to the
directory containing `DSLtk.hpp`. There is no separately compiled C++ library
to link for the default facilities. The embedded enum-reflection headers are
already present in the file. Optional Lua integration is a separate build
choice whose macro and link requirements are explained in Chapters 2 and 34.
Keeping the first example independent of Lua makes the basic installation
contract easy to verify.

This tiny example is intentionally ordinary. Its value is that it gives a
reader a working baseline before introducing template diagnostics or mutable
parsing state. A useful development habit is to retain such a baseline as the
language grows. If an application suddenly fails to include the header, the
baseline helps distinguish an environment change from a problem in the new
grammar or domain representation.

## Composition describes an interface, not a universal capability system

The C++ `DSL<Derived, Features...>` base inherits a nested mixin specialization
for each selected feature. A nonempty mixin can add methods or operator
friends that use the derived class's domain members. A marker mixin can
instead record feature intent while the substantive facility remains a free
function or standalone type. The architecture chapter lists both kinds and
explains the concepts used to inspect feature membership.

This distinction prevents a common misunderstanding: selecting an AST feature
does not make every object an AST node, and omitting it does not prohibit
calling a free AST factory. Likewise, a marker cannot prove that an arbitrary
tree is well typed. The feature list is useful interface metadata and a
composition mechanism, but an application still needs representations whose
invariants match its domain.

Name lookup and overload resolution also remain ordinary C++ mechanisms.
Two custom mixins can introduce conflicting names. A hidden friend can be
found through argument-dependent lookup without becoming an inherited member.
A callable can fail its requirements only when a member template is actually
instantiated. These are not exceptional situations peculiar to DSLtk; they
are the host-language rules through which the embedded language operates.

The D port reaches a similar goal with a template mixin rather than a
recursive base class. Native string template arguments replace many uses of
the C++ fixed-string wrapper. These differences make Chapter 37 useful even
for a reader who writes only C++: they reveal which contracts belong to the
toolkit's idea and which arise from one language's implementation technique.

## Four representations worth keeping distinct

The toolkit offers several tree-like structures. `ASTNode` is a tagged,
value-type tree with textual leaf payloads. `SExpr` represents symbolic atoms
and lists and has a reader and printer. `Term` represents first-order
variables and function applications for substitution and reduction. An
e-graph groups equivalent expressions rather than storing only one chosen
tree. Their overlapping vocabulary does not make them interchangeable.

An AST is convenient when the application wants arbitrary tags and custom
predicates. An S-expression is useful as compact interchange notation and
as the language for equality rules. Terms make variable binding and
unification explicit. An e-graph retains alternatives that a destructive
rewrite sequence would discard. Converting between these forms can be useful,
but each conversion has a contract about heads, leaves, variables, and payloads.

For example, the diagnostic dump of a tree is not automatically a stable
serialization format. A printed string may omit distinctions that matter
when reconstructing values or may depend on formatting policy. Chapter 30
instead gives user codecs responsibility for choosing their external format.
The binary case study makes byte order, length bounds, and version explicit
rather than assuming a C++ object's memory layout is a portable record.

Choosing the representation first also clarifies error handling. A malformed
tree can be rejected during validation, before rewriting. A failed parse can
retain an offset without inventing a placeholder semantic value. A limited
saturation run can return an operational stopping reason without denying
equalities already established. Each distinction is easier to express when
the result type preserves the information callers need.

## Ownership is part of the language implementation

A parser input often borrows a source string. A match can contain a view of
that source. A callback may capture a token vector by reference. A copied
AST owns its descendants, whereas a reference to a rule stored in a growing
vector can become invalid. These facts determine whether a pipeline remains
correct after it leaves the function where its pieces were assembled.

Before connecting two stages, ask whether the second stage receives an
owned value, a mutable reference, or a borrowed view. Then identify the
object that keeps the referenced storage alive. This is more informative
than labeling an interface zero-copy. Avoiding a copy is useful only if the
resulting lifetime remains well defined for the caller's actual use.

The configuration study copies token text out of the preprocessed string.
The binary study copies a payload into its returned frame. Both could use
more elaborate shared-buffer ownership, but the small bounded data makes an
owned representation straightforward. In contrast, copying a whole large
query tree on each transformation may become a performance concern. The
appropriate ownership model depends on the workload and the desired API,
not on a universal preference for copying or borrowing.

Mutability deserves the same explicit treatment. A lazy value caches its
first result; memoization stores results by key; preprocessing changes macro
state; a parser changes its cursor. Reusing such objects can be intentional,
but reusing them across logically independent operations changes the state
contract. The chapters identify where a fresh object is simpler than trying
to restore a partially modified one.

## Measure the work that actually happens

Compile-time machinery can remove runtime decisions, but it can also increase
compilation work. The enum-reflection implementation discovers values over a
configured range. Static regex construction prepares an automaton while
runtime matching still walks input bytes. CRTP removes the need for a virtual
feature interface, but other facilities use `std::function`, containers, and
owned strings. These mechanisms have different cost models.

Similarly, deferred arithmetic does not imply element-wise array fusion.
The C++ expression nodes evaluate whole operands and call their operators.
If those operators allocate intermediate arrays, deferring their invocation
does not remove those allocations. The D implementation provides a different
indexed evaluation path. An accurate performance discussion names the
operation being deferred, the storage it creates, and the point at which the
work is finally performed.

The case studies provide correctness evidence rather than benchmarks. A
pipeline's readability is useful even when it compiles to the same work as
direct function calls. A rewrite can simplify a representation without
improving an application's dominant runtime cost. Performance claims should
follow measurements on representative input, with compilation time and memory
included when those are important to the user of the language.

## Reading examples as evidence

A complete program demonstrates that a particular combination of APIs builds
and runs. A source excerpt explains an implementation mechanism but may omit
its enclosing class. A negative example demonstrates a rejected program. A
pseudocode block states control flow without claiming that its words are an
exported API. The book labels these forms so a reader can use each for its
appropriate purpose.

Assertions should cover independent obligations. A units result needs both
the right dimension and the right conversion factor. A query optimization
needs a changed tree and preserved results. A binary codec needs more than a
round trip, because mutually incorrect encoders and decoders can agree with
each other. Concrete wire bytes and truncation checks provide stronger
evidence for the format boundary.

The supplied implementation remains the final reference when prose and code
disagree. This is why the book describes snapshot defects directly and keeps
small compile probes for them. An expected compiler rejection is a useful
regression observation when it is explicitly identified as such; presenting
that same program as a working example would mislead the reader.

The next chapter establishes the build environment and a set of first
programs. After that, the architecture and foundation chapters explain why
the small example above works. Keeping those two perspectives together—an
executable consumer and a careful reading of the implementation—is the most
reliable way to grow a language from a few useful expressions into a
maintainable tool.


## Implementation map and review

The following diagram summarizes the chapter's central dependency or state
transition. Arrows denote logical dependence, not elapsed time or measured
performance. The contracts below separate what the library provides from the
conditions that the application must establish.

```{=latex}
\begin{figure}[htbp]
\centering
\begin{tikzpicture}
\node[dslsmall] (n0) {Domain};
\node[dslsmall, right=5mm of n0] (n1) {Notation};
\draw[dsledge] (n0) -- (n1);
\node[dslsmall, right=5mm of n1] (n2) {Representation};
\draw[dsledge] (n1) -- (n2);
\node[dslsmall, right=5mm of n2] (n3) {Meaning};
\draw[dsledge] (n2) -- (n3);
\end{tikzpicture}
\caption{The four decisions behind a DSL.}
\label{fig:atlas-1}
\end{figure}
\begin{table}[htbp]
\centering\small
\begin{tabular}{L{0.28\linewidth}L{0.61\linewidth}}
\toprule Contract & Interpretation \\
\midrule
Vocabulary & Domain operations determine the useful constructs. \\
Representation & A syntax tree preserves choices for later processing. \\
Meaning & Application code supplies the interpretation. \\
\bottomrule
\end{tabular}
\caption{Implementation contracts for this chapter.}
\label{tab:atlas-1}
\end{table}
\begin{pseudocode}[Operational summary]
\textbf{1.} Identify a repeated domain operation.\\
\textbf{2.} Choose an embedded or external notation.\\
\textbf{3.} Represent its operands explicitly.\\
\textbf{4.} Validate before interpretation.\\
\textbf{5.} Compare the result with the domain requirement.
\end{pseudocode}
```

### Review questions

1. Which part of your proposed language belongs in DSLtk and which part belongs in domain code?

2. Would an ordinary function API express the same requirement more simply?
