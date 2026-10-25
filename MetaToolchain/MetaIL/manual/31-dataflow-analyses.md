# 31. Variable and expression analyses

`LiveVariables` computes block live-in/live-out sets with iterative backward
dataflow. `ReachingDefinitions` computes which assignments can reach block
boundaries; definitions are identified using variable names and statement positions.
`BlockDefUse` exposes per-block definitions and uses.

```sh
build/metail analyze examples/dataflow.mi \
  --passes LiveVariables,ReachingDefinitions,BlockDefUse
```

These analyses operate on mutable local names, not SSA values. Their treatment
of assignments and CFG joins is therefore important: two assignments to `x` are
different reaching definitions even though later expressions use the same name.
Unreachable predecessors are excluded from reachable reaching-definition facts.

Other useful registered analyses include `VariableUses`, `DefinitionCounts`,
`UnusedDefinitions`, and `ParameterUses`. Expression inspection is available through
`ExpressionNodeCount`, `MaximumExpressionDepth`, `ExpressionRedundancy`, and
`ExpressionPurity`. `OpcodeHistogram`, `StatementHistogram`, and the `Count...`
families summarize operators and statements.

`CallGraph` and `ImpureCallSites` describe syntactic call relationships and sites;
they are not interprocedural execution or alias analyses. `LiteralValueRange`
summarizes literal values appearing in the IR. It does not prove possible runtime
ranges for arbitrary variables or arithmetic expressions.

Use reports to understand a program or drive an application-specific decision,
but do not silently treat a syntactic statistic as an optimization proof. The
registered rewrite guards currently consult their own supported literal/purity
conditions rather than arbitrary JSON analysis results.

[Contents](README.md) · [Next: Statistics](32-numeric-statistics.md)
