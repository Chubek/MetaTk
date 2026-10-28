# 19. Analysis and inspecting program structure

Use analysis to answer a concrete question: which blocks are reachable, which
locals remain live, where definitions reach uses, or what conservative integer
facts are available. An analysis report is not itself a transformed program.

## Analyze a branch with a join

<!-- check: juniper -->
```juniper
@module(analyze_me)
@fn(adjust, (x, enabled))
  @let(value, x)
  @br(enabled, changed, unchanged)
@label(changed)
  @let(value, add(value, 1))
  @goto(join)
@label(unchanged)
  @goto(join)
@label(join)
  @return(value)
@end
```

The initialization before the branch makes `value` available on both paths.
Save this program as `$WORK/analyze.jun`, then request targeted reports:

```sh
"$J" analyze "$WORK/analyze.jun" \
  --passes ControlFlowGraph,Dominators -o "$WORK/control.json"
"$J" analyze "$WORK/analyze.jun" \
  --passes LiveVariables,ReachingDefinitions -o "$WORK/dataflow.json"
python3 -m json.tool "$WORK/control.json"
```

These are MetaIL analyses. Their payload schema belongs to the selected MetaIL
runtime, not the machine-IR JSON schema described in chapter 28. Inspect the
report you receive before writing a consumer around a particular payload shape.

## Use small consumers rather than parsing display text

This Python program loads a generated report and shows its top-level structure:

<!-- check: python -->
```python
import json
import sys

with open(sys.argv[1], encoding="utf-8") as stream:
    report = json.load(stream)
if not isinstance(report, dict):
    raise SystemExit("expected an analysis report object")
for key, value in report.items():
    print(key, type(value).__name__)
print(json.dumps(report, indent=2, sort_keys=True))
```

Save it as `show-analysis.py` and pass `$WORK/control.json`. Avoid regular
expressions over pretty-printed JSON; use typed values and explicit assertions.

## Integer facts versus syntactic statistics

```sh
"$J" analyze "$WORK/analyze.jun" --passes WeighstoneIntegerFacts \
  -o "$WORK/integer-facts.json"
"$J" analyze "$WORK/analyze.jun" --passes NumericLiteralsMean \
  -o "$WORK/literal-statistics.json"
```

`WeighstoneIntegerFacts` computes conservative block-local value information.
Facts reset at labels and terminators; they are not a whole-program proof that
all inputs satisfy a condition. Numeric literal statistics describe program
syntax, and their floating-point report values are not new scalar IR types or
integer rewrite facts.

## Compare stages fairly

Analyze the same saved canonical input when comparing optimizers. First record
the unoptimized report, optimize into a separate file, then analyze that output.
`analyze --optimize` is not a supported flag combination; explicitly create the
optimized IR first. Likewise, `--passes` on analysis selects analyzers, not
scheduler passes or instruction-selection rules.

For a C++ host, `Compiler::analyze(module, passes)` returns the JSON as an owned
`std::string`. Parse it using your application's JSON library if needed; Juniper
does not require a new JSON dependency just to provide this interface.

[Previous](18-optimization.md) · [Index](README.md) · [Next](20-cpp-contexts.md)
