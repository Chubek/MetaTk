# 8. Terms, matching, and rewriting

## Term model

`rewrite::Term` is an owned value with three kinds:

* atom: an arbitrary string;
* node: a nonempty symbol and ordered child terms;
* variable: a nonempty variable name.

```cpp
using namespace rewrite;
Term subject = node("add", {atom("42"), atom("0")});
Rule zero("add-zero", node("add", {var("x"), atom("0")}), var("x"));
System rules;
rules.add(zero);
auto result = rules.normalize(subject);
```

Equality is structural, including kind. Atom `"f"` differs from node `f()`.
The default term is an empty atom. Empty node symbols and empty variable names
are construction errors. Mutable child access is for nodes; attaching children
to an atom/variable violates the term invariant and normalization rejects it.

Terms are first-order: there are no binders, lambda capture, associative-
commutative matching rules, variadic sequence variables, or implicit numeric
evaluation. Atom `"042"` differs from atom `"42"`. Application-specific numeric
or semantic transformations should be explicit passes.

## Matching and substitution

`match(pattern, subject, substitution)` matches variables to arbitrary terms.
A repeated variable must match exactly the same term each time. Existing
bindings constrain subsequent matches.

Failure is transactional: the caller's substitution remains unchanged. This
matters when trying alternatives; a failed earlier child cannot leave a new
binding that corrupts a later rule attempt. `substitute` recursively instantiates
variables and diagnoses an unbound variable instead of silently treating it as
an atom.

Rule construction rejects a bare-variable left-hand side and any right-hand
variable not present on the left. Matching is capture-free because this model
does not have lexical binders. Introducing language binding constructs requires
an application-level representation and hygiene policy.

## Rule ordering and normalization

Rules are tried in insertion order. At a selected root, the first matching rule
that changes the term is applied. A rule that returns an identical term is a
no-op and does not falsely trigger an endless identity rewrite or consume the
normalization budget.

`Direction::BottomUp` visits children before a parent. `TopDown` visits a parent
first. Each normalization iteration performs exactly one actual replacement.
The step count therefore means rule applications, not tree sweeps. Newly
created trees are not recursively rewritten inside one unbounded step.

This makes growth rules finite under a step budget. For example, `f($x) =>
f(f($x))` reaches a limit rather than overflowing the stack inside one nominal
top-down step. There is no claim that arbitrary rewrite systems terminate.

The compatibility option `normalize_children_after_rewrite` remains present,
but replacement children are now revisited through subsequent bounded
traversals in both settings. Code that depended on the original sweep-counting
behavior should use actual `Result::steps` and the documented one-step model.

## Limits and cycles

Default options are bottom-up traversal, 10,000 replacements, cycle detection,
maximum term depth 256, and maximum node count 1,000,000. The initial term and
each replacement result are checked against structural limits.

Cycle detection uses an unambiguous structural key, not the human printer's
output or a hash alone. Cycles such as `a() => b()` followed by `b() => a()` are
reported distinctly from a step limit. Stored history uses memory proportional
to the sizes of seen terms; turn off cycle detection if a trusted workload
prefers a strict step-only budget.

At a zero/exhausted budget, a term already in normal form is still recognized
as normalized. Otherwise `step_limit_reached` is set. `Result` also includes the
last term, step count, `normalized`, `cycle_detected`, and an `error` string for
structural failures. The facade maps non-normal outcomes into diagnostics while
retaining the last term when useful.

## Events

Subscribe to a callback or a `Listener::on_rewrite` implementation. Events
describe applied rules, before/after terms, step number, and finish/limit/cycle
conditions. Event snapshots own their term values. Copying large before/after
trees for tracing can be expensive; avoid listeners when that information is
not needed.

Direct `rewrite_root` performs one root replacement and emits its event. It
does not implement the full normalization resource policy by itself. Use
`normalize` for untrusted-depth or budgeted traversals. Listener exceptions
propagate and listener references must remain alive.

## Textual terms and rules

`metachain::text::parse_term` uses a DSLtk parser wrapper and returns a term or a
parse error. Syntax examples:

```text
42
"arbitrary atom with spaces"
$variable
add(42, 0)
empty()
"constructor with spaces"(child)
```

Quoted atoms use the same strict escapes as assembly strings. Nodes always
print parentheses, even with zero children. The printer escapes control bytes
and quotes symbol spellings as needed. For conventional variable names and
terms produced by the text parser, printing and parsing preserve structure.

A rule occupies one line:

```text
zero: add($x, 0) => $x
one: mul($x, 1) => $x
```

Names are identifiers. Blank lines and `;`/`#` comments outside strings are
accepted. Invalid RHS variables fail during textual rule construction.
`parse_rules` returns parsed rules plus the first error's line and offset.

The `rewrite` command places rules before `%%` and the subject after it:

```text
zero: add($x, 0) => $x
%%
add(add(42, 0), 0)
```

This prints `42`. Rule text is data; it is never evaluated as C++, Lua, or a
shell command.
