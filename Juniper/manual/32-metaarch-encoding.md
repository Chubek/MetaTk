# 32. Checked instruction encoding with MetaArch

The optional adapter is an encoding boundary. It calls the real
`metaarch::Spec::encode_form` with explicit form, parameter, and physical-register
maps. It is not a register allocator, an assembler-dialect guesser, or a complete
function code generator.

## Enable and link the adapter

In a source build configure `-DJUNIPER_WITH_METAARCH=ON`, then link
`Juniper::MetaArch` and include `<juniper/MetaArch.hpp>`. The ordinary installed
Juniper package exports only the core library/executable. Installed adapter users
must separately supply the MetaArch headers and DSL/C libraries; do not assume an
exported `Juniper::MetaArch` package target exists there.

## A complete synthetic encoding example

The tiny architecture below is invented for this demonstration. Its expected
bytes are defined by its own encoder, not an unverified claim about a real ISA.

<!-- check: cpp metaarch -->
```cpp
#include "Support.hpp"
#include <juniper/MetaArch.hpp>
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <vector>

int main(int argc, char** argv) {
    try {
        const char* home = argc > 1 ? argv[1] : nullptr;
        juniper::Compiler compiler(home);
        auto module = example::take(compiler.parse(R"(
            backend toy extends portable {
              rule add_immediate : add(reg,imm(-128,127)) => ADDI cost 1;
            }
            fn decrement(x) { return add(x,-1); }
        )"));
        auto machine = example::take(juniper::select(module, home));
        const auto& instructions = machine.functions.at(0).instructions;
        const auto found = std::find_if(instructions.begin(), instructions.end(),
            [](const auto& inst) { return inst.opcode == "ADDI"; });
        example::expect(found != instructions.end(), "ADDI not selected");

        auto specification = metaarch::Spec::parse_dsl(R"(architecture {
          schema = 1;
          target { arch = toy; addr_size = 64; byte_order = little;
                   obj_format = ELF64; }
          register r0 { class = gpr; width = 64; enc = 0; }
          register r1 { class = gpr; width = 64; enc = 1; }
          mnemonic addi {}
          encoder addi {
            body = "REQUIRE dst >= 0 && dst <= 1\nREQUIRE src >= 0 && src <= 1\nEMIT8 0x10 | dst\nEMIT8 src\nEMIT8 (imm & 255)\n";
          }
          form { mnemonic = addi; encoder = addi; operands = [
            {kind = reg; class = gpr; width = 64;},
            {kind = reg; class = gpr; width = 64;},
            {kind = imm; width = 8; signed = true;
             imm_min = -128; imm_max = 127;}
          ]; }
        })");
        if (!specification) {
            for (const auto& diagnostic : specification.error()->diagnostics)
                std::cerr << diagnostic.message << '\n';
            return 1;
        }

        juniper::FormBinding mapping{0, {"dst", "src", "imm"}, {-1, 0, 1}};
        juniper::RegisterBindings registers{{found->result, "r1"}, {"x", "r0"}};
        auto bytes = example::take(juniper::encode_instruction(
            *specification.get(), *found, mapping, registers));
        example::expect(bytes.bytes == std::vector<std::uint8_t>{0x11, 0, 0xff},
                        "synthetic encoding differs");
        for (auto byte : bytes.bytes)
            std::cout << std::hex << std::setw(2) << std::setfill('0')
                      << static_cast<unsigned>(byte);
        std::cout << '\n';

        auto invalid = *found;
        invalid.operands.at(1).value = 128;
        example::expect(!juniper::encode_instruction(
            *specification.get(), invalid, mapping, registers),
            "out-of-range immediate was accepted");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
```

## Read the map precisely

Form index `0` is explicit because this synthetic specification declares exactly
one form. `parameters` names encoder parameters in **declared form operand
order**. `operand_order` entries mean `-1` for the instruction result and
nonnegative indices for captured instruction operands. Do not infer this mapping
from encoder VM first-use order or from an opcode's spelling.

The adapter maps virtual register names to supplied physical register names and
delegates form validation. Missing assignments, bad indices, mismatched mapping
lengths, invalid classes/widths/immediates, and backend encoding errors fail with
`JUN-E500`. Label/symbol operands are rejected at this boundary; returned
relocations remain explicit owned records rather than being silently resolved.

This example encodes the arithmetic instruction, **not** the `RET` pseudo or a
whole callable function. A complete native backend must still specify allocation,
ABI conventions, control lowering, relocation handling, and verified semantics
for every emitted instruction.

[Previous](31-cpp-pipeline.md) · [Index](README.md) · [Next](33-cmake.md)
