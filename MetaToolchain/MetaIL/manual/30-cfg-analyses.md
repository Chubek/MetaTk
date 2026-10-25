# 30. Control-flow analyses

Control-flow analyses operate on the current program and do not mutate it. Select
them by registered kernel name:

```cpp
auto report = context.analyze(module,
    "ControlFlowGraph,Dominators,PostDominators,NaturalLoops,UnreachableBlocks");
```

`ControlFlowGraph` reports block names, statement ranges, predecessors, successors,
and reachability. `ReachableBlocks` and `UnreachableBlocks` expose reachability
directly. `BranchTargets` and `TerminalBlocks` summarize edges and terminal regions.

`Dominators` computes the usual entry-relative relation for reachable control
flow. Unreachable predecessors do not contaminate reachable results. Treat facts
about unreachable regions conservatively when consuming the report.

`PostDominators` accounts for regions with no path to an exit. Unreachable blocks
and blocks in nonterminating regions conservatively report only themselves; an
exit is not incorrectly claimed to postdominate a conditional branch into an
infinite successor region.

`NaturalLoops` and `LoopHeaders` identify structural loops, including natural
self-loops. They do not prove iteration counts, termination, or induction-variable
properties. `CriticalEdges` and `CyclomaticComplexity` provide additional graph
summaries useful for inspection.

Reports are JSON, generally organized by function and block. Inspect the actual
schema produced by a small fixture before building a downstream report consumer.
Remember that statement indices in these reports are one-based.

[Contents](README.md) · [Next: Dataflow analyses](31-dataflow-analyses.md)
