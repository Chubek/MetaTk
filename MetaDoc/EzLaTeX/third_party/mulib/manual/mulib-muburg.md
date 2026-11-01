# mulib-muburg

Costed bottom-up tree-grammar instruction selection. This is a small dynamic
programming selector, not an alias for a tree walk or an implementation of a
particular BURG source-language parser.

## Integration and grammar

Include `mulib-muburg.h`; explicitly define `MULIB_IMPL_MUBURG` once. No other
modules are required. Declarations use C linkage in C++.

Input nodes contain an application-defined integer terminal `opcode`, arity
0/1/2 and child node IDs. Child IDs may be in any array order. Only used children
are read; unused child fields are ignored. Acyclic sharing is allowed, but costs
and emission treat the graph as an **expanded tree**, not a register-sharing DAG.
All nodes, even outside the requested root, are checked for cycles.

Each rule has this meaning:

```
lhs_nonterminal <- opcode(child_goal[0], child_goal[1]) : cost, instruction_tag
```

`lhs` and used child goals must be in `[0, goal_count)`. Rule arity and terminal
must match the node exactly. Costs are nonnegative `uint64_t`; the instruction
tag is an unsigned user-defined backend token. Terminals and nonterminals are
distinct domains. Patterns have one terminal root; nested patterns are expressed
by additional nonterminals/rules. No epsilon rules, nonterminal chain rules,
wildcards, semantic predicates or textual rule parser are supported. Encode
immediate ranges/type classes as distinct terminals before selection if needed.

## Selection API and optimality

Zero initialize `mulib_muburg_plan_t`, then call:

```c
mulib_muburg_select(nodes, node_count, rules, rule_count, goal_count,
                   root, requested_goal, limits, &plan, diagnostic);
```

The dynamic program computes the least additive cost per `(node,goal)`, visiting
children before parents. A rule's cost includes each selected child-goal cost.
This is minimum-cost selection for the stated tree grammar, not greedy rule
matching. Equal-cost alternatives retain the earliest rule. Zero costs and an
exact cost of `UINT64_MAX` are valid; a separate choice sentinel distinguishes
uncovered states. Overflowing alternatives are ignored when a finite alternative
covers the requested root. If structural covers exist but all exceed the cost
range, return OVERFLOW; otherwise an uncovered root returns NO_COVER.

On success, `plan.cost` is the root cost. `choices[node*goal_count+goal]` indexes
the selected rule, or equals `SIZE_MAX` when unavailable. Its corresponding cost
is meaningful only when a choice exists. Plan arrays are owned, but node/rule
arrays remain borrowed and **must stay immutable and alive** until emission/free.
Use `mulib_muburg_free` for the owned tables; do not copy a live plan. Output
must be empty; selection failure frees scratch and does not fill output.

## Emission

`mulib_muburg_emit(&plan, emit_limit, callback, user, &emitted)` expands the
selected cover in left-child/right-child/parent order. Each callback receives
the original node ID, selected rule ID and instruction tag. A DAG node used
twice invokes the callback twice; a tag of zero is still delivered. The client
interprets tags and emits actual target instructions, or transforms them into
common MIR. The module deliberately does not assume any unfinished emitter API.

A callback returning zero stops with STOPPED. `emit_limit` counts callback
invocations; its zero value prohibits any invocation. `emitted` includes the
callback that stopped. STOPPED/RESOURCE_LIMIT leave observable partial output
at the callback; emission is not transactional. The plan itself is unchanged.
Traversal uses an explicit stack bounded by node count, not recursion.
Emission checks descriptor-size products and each visited rule/node/child-goal
combination before indexing or descending. Malformed plans return INVALID (or
RESOURCE_LIMIT for unrepresentable dimensions), possibly after partial callbacks.
These checks do not relax the borrowed arrays' lifetime/immutability requirement
or verify that caller-forged pointers have the declared allocation sizes.

## Limits and failures

`limits` bounds nodes, rules, nonterminal goals and total DP cells separately.
Both the `node_count*goal_count` multiplication and allocation-byte products are
checked. A zero node count or goal count is INVALID; a nonempty tree with no
rules returns NO_COVER. Other statuses are CYCLE, RESOURCE_LIMIT, NO_MEMORY,
OVERFLOW, STOPPED and OK. Diagnostics locate a node (or a rule index for invalid
grammar). Status strings are static. Successful diagnostic locations are ignored.

DP memory is O(nodes*goals): one `uint64_t` cost, one `size_t` choice and one
overflow byte per cell, plus one processed byte per node. Emission needs one
frame per input node. Time is O(nodes² + nodes*rules); each rule has at most
two children. Expanded emission can be exponential for shared DAGs, which is
why its separate explicit callback bound is mandatory.

## Runnable example

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude examples/modules/muburg.c -o /tmp/muburg
/tmp/muburg
```

The example has register and immediate nonterminals, chooses ADD-with-immediate
over a more expensive register/register cover, and asserts emitted tags and cost.
Tests also exercise malformed graph IDs, cycles, no cover, overflow and emission
limits/callback cancellation. They also interpret selected immediate/register
instruction tags into real MINIVM bytecode and compare execution with common
MIR lowered through CMACHINE.
