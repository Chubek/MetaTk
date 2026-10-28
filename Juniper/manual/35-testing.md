# 35. Testing and reproducible backend development

Test each contract independently: source validity, semantic preservation, selected
cover, scheduling invariants, and encoded bytes. A green parser test does not
prove a backend is complete or correct.

## Run the supplied tests and manual checks

```sh
ctest --test-dir "$JUNIPER_ROOT/build" --output-on-failure
python3 "$JUNIPER_ROOT/manual/check_examples.py" \
  --juniper "$J" --metail-home "$METAIL_HOME"
python3 "$JUNIPER_ROOT/manual/check_examples.py" \
  --juniper "$J" --metail-home "$METAIL_HOME" \
  --cpp --with-metaarch --build-dir "$WORK/manual-check"
```

The checker extracts marked programs from the actual Markdown, checks expected
failures, verifies canonical round trips for positive source examples, and can
compile/run complete C++ examples. It is not a language interpreter. Python
artifact-inspection snippets are syntax-checked; supplied fixture scripts are
also exercised against generated outputs.

## A complete scheduling invariant test

<!-- check: cpp -->
```cpp
#include "Support.hpp"
#include <iostream>
#include <map>
#include <vector>

int main(int argc, char** argv) {
    try {
        const char* home = argc > 1 ? argv[1] : nullptr;
        juniper::Compiler compiler(home);
        auto module = example::take(compiler.parse(R"(
            backend test extends portable { issue 2; resource alu 2; }
            fn pair(a,b,c,d) { return add(mul(a,b),mul(c,d)); }
        )"));
        auto selected = example::take(juniper::select(module, home));
        auto scheduled = example::take(juniper::schedule(selected));
        const auto& function = scheduled.functions.at(0);
        std::vector<const juniper::Instruction*> by_id(function.instructions.size());
        for (const auto& inst : function.instructions) {
            example::expect(inst.id < by_id.size(), "bad stable ID");
            example::expect(inst.cycle.has_value(), "missing cycle");
            by_id[inst.id] = &inst;
        }
        std::map<std::uint64_t, unsigned> issued;
        std::map<std::uint64_t, std::map<std::string, unsigned>> resources;
        for (const auto& inst : function.instructions) {
            const auto cycle = *inst.cycle;
            example::expect(++issued[cycle] <= scheduled.backend.issue_width,
                            "issue width exceeded");
            example::expect(++resources[cycle][inst.resource] <=
                            scheduled.backend.resources.at(inst.resource),
                            "resource capacity exceeded");
            for (auto dependency : inst.dependencies) {
                const auto& producer = *by_id.at(dependency);
                example::expect(cycle >= *producer.cycle + producer.latency,
                                "dependency result used too early");
            }
        }
        const auto repeated = example::take(juniper::schedule(scheduled));
        example::expect(juniper::emit_machine_json(repeated) ==
                        juniper::emit_machine_json(scheduled),
                        "rescheduling changed deterministic output");
        std::cout << "schedule invariants passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
```

## Semantic and boundary tests

For arithmetic, use signed-minimum, signed-maximum, `-1`, `0`, `1`, and values
around immediate/mask/shift boundaries. Add fixed-seed random cases. A useful
reference conversion in Python is:

<!-- check: python -->
```python
def signed64(value):
    bits = value & ((1 << 64) - 1)
    return bits - (1 << 64) if bits >= (1 << 63) else bits

def wrapping_madd(x, y, z):
    return signed64(x * y + z)

assert wrapping_madd((1 << 63) - 1, 2, 1) == -1
```

Comparing this expression with your custom opcode executor is meaningful; merely
computing the expression twice is not a backend test. Track call traces and trap
outcomes as well as values, particularly for lazy select and nested expressions.
For real ISA encoding tests, use independently verified bytes/assemblers where
available. Do not promote a synthetic toy fixture into an ISA correctness claim.

Run sanitizers in a separate build and keep source, runtime/kernel version,
configuration, pass list, and profile revision with any golden artifacts. Assert
stable semantic properties rather than incidental whitespace or temporary-number
choices unless byte-for-byte canonicalization is specifically what you test.

[Previous](34-diagnostics-and-security.md) · [Index](README.md) · [Next](36-capstone.md)
