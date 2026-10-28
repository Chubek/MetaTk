# 31. An end-to-end C++ backend pipeline

An embedding pipeline should preserve its input, select a deliberately chosen
optimization policy, carry its backend profile through canonicalization, and
check each stage's result. Keep the MetaIL runtime root consistent across the
context and free-function selection.

## A complete pipeline

<!-- check: cpp -->
```cpp
#include "Support.hpp"
#include <algorithm>
#include <iostream>

int main(int argc, char** argv) {
    try {
        const char* home = argc > 1 ? argv[1] : nullptr;
        juniper::Compiler compiler(home);
        auto original = example::take(compiler.parse(R"(
            backend pipeline extends portable {
              issue 2;
              resource alu 2;
              rule fused : add(mul(reg,reg),reg) => MADD
                cost 3 latency 4 resource alu;
            }
            module pipeline_demo;
            fn affine(x, factor, bias) {
              return add(mul(x,factor),add(bias,0));
            }
        )", "pipeline.junir"));
        const auto snapshot = example::take(compiler.emit(original));
        auto optimized = example::take(compiler.optimize(
            original, "ConstantFolding,AddZeroRight", 2));
        example::expect(example::take(compiler.emit(original)) == snapshot,
                        "optimization modified its input");

        auto selected = example::take(juniper::select(optimized, home));
        const auto& instructions = selected.functions.at(0).instructions;
        example::expect(std::any_of(instructions.begin(), instructions.end(),
            [](const auto& inst) { return inst.opcode == "MADD"; }),
            "expected the fused multiply-add cover");
        auto scheduled = example::take(juniper::schedule(selected));
        example::expect(scheduled.scheduled, "scheduling flag missing");
        for (const auto& function : scheduled.functions)
            for (const auto& instruction : function.instructions)
                example::expect(instruction.cycle.has_value(), "cycle missing");

        auto canonical = example::take(compiler.emit(optimized));
        auto reloaded = example::take(compiler.parse(canonical));
        example::expect(example::take(compiler.emit(reloaded)) == canonical,
                        "canonical round trip changed the profile or program");
        std::cout << juniper::emit_machine_json(scheduled);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
```

The selected pass list removes the inner `add(bias, 0)` and exposes exactly the
pattern expected by the custom profile. The code deliberately does not say
`all` and then assume every future runtime version leaves the same tree shape.

## Insert application policies at explicit boundaries

Before selection, check policies that the canonical validator does not prove:
allowed functions/callees, definite assignment, accepted control flow, and your
backend's scalar-width contract. After selection, inspect virtual instruction
coverage and any unsupported pseudos. Before encoding, require physical register
assignments, ABI/control lowering, and explicit form bindings.

Do not mutate a module or machine vector while another stage holds references
into it. The APIs return independent values, which makes a staged pipeline easy
to reason about. Moving a large completed value is often preferable to copying
it unnecessarily, but input validation and effect reasoning come first.

## Record artifacts when debugging

In a production host, save canonical source before/after optimization and the
selected/scheduled machine forms when investigating a failure. Do not conflate
analysis JSON with machine JSON, and do not discard the original diagnostic code
when adding application context.

The code above stops at scheduled virtual IR. The next chapter intentionally
encodes only a specifically mapped instruction, rather than pretending that the
whole function has acquired physical registers and an executable ABI.

[Previous](30-hazards-and-fences.md) · [Index](README.md) · [Next](32-metaarch-encoding.md)
