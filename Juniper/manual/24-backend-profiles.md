# 24. Backend profiles and inheritance

A backend profile specifies tree-selection rules and scheduling resources. It
does not change the scalar language's type or arithmetic semantics. Profiles are
ordinary C++ values or Juniper declarations; Juniper has no YAML/TOML/JSON profile
loader to configure.

## Extend the portable profile in source

<!-- check: juniper mode=select opcodes=ADDI -->
```juniper
@backend(two_wide)
  @issue(2)
  @resource(alu, 2)
  @rule(add_small, add(reg, imm(-2048, 2047)), ADDI, 1, 1, alu)
@end
@module(profile_demo)
@fn(adjust, (x))
  @return(add(x, 7))
@end
```

`@backend(two_wide)` expands to `backend two_wide extends portable {`. It imports
the portable rules before appending local rules. In this example an in-range
literal selects `ADDI` rather than requiring a separate constant instruction and
register-register add.

Equivalent declaration syntax, as a **metadata fragment**, is:

```text
backend two_wide extends portable {
  issue 2;
  resource alu 2;
  rule add_small : add(reg, imm(-2048, 2047)) => ADDI
    cost 1 latency 1 resource alu;
}
```

All metadata must precede the program body. Issue/resource widths are `1..64`;
cost and latency are positive integers through `1000000`. A rule without explicit
attributes defaults to cost `1`, latency `1`, resource `alu`.

## Portable defaults and override rules

The portable profile has `alu` and `control`, both capacity 1, and issue width 1.
Literal materialization costs 1. Ordinary scalar operator rules cost 2; multiply
has latency 3 and other portable arithmetic rules latency 1. They all use `alu`.
Structural operations are emitted separately by the lowerer.

Local resource declarations can replace inherited capacities. Local rules cannot
replace an inherited rule by reusing its name: duplicate names are errors.
Equal-cost selection ties favor the first declaration, so an appended rule must
actually be preferable in total cost to defeat an inherited cover.

Adding `resource mul 1;` by itself does not reroute multiplication to that unit.
Write a winning rule that names `mul`, or edit the inherited rule in C++.

## Profiles as C++ values

```cpp
auto backend = juniper::Backend::portable();
backend.name = "custom_cpu";
backend.issue_width = 2;
backend.resources["mul"] = 1;
for (auto& rule : backend.rules) {
    if (rule.name == "base_mul") {
        rule.resource = "mul";
        rule.latency = 4;
    }
}
// module.backend = std::move(backend); then compiler.check(module).
```

A default `Module` has the portable profile. A default `Backend{}` does **not**
load rules; the factory call matters. Likewise, raw `backend minimal { ... }`
without `extends portable` starts with required resources but no arithmetic or
literal rules. Use that form only when you intend to supply complete coverage
for the program subset you accept.

`alu` and `control` resources must remain available for structural instructions.
Validate profiles before publishing them, and treat every custom opcode's meaning
as a semantic assertion that your later backend must implement.

[Previous](23-preprocessing-api.md) · [Index](README.md) · [Next](25-selection-costs.md)
