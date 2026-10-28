#pragma once
#include <juniper/Juniper.hpp>
#include <MetaArch.hpp>
#include <MetaArchIR/MetaArchIR.hpp>
#include <span>

namespace juniper {

// Explicit mapping from a selected instruction to a checked architecture form.
// operand_order entries: -1 = result, >=0 = instruction operand index. Parameters
// are in the form's declared operand order, never inferred from VM bytecode.
struct FormBinding {
    std::size_t form = 0;
    std::vector<std::string> parameters;
    std::vector<int> operand_order;
};
using RegisterBindings = std::map<std::string, std::string, std::less<>>;

// This is an encoding boundary, NOT a register allocator or an ABI. All virtual
// registers referenced by the mapping must already have physical assignments.
// Symbolic/label inputs are rejected; returned relocations remain explicit.
inline Result<metaarch::Encoded> encode_instruction(const metaarch::Spec& spec,
    const Instruction& instruction, const FormBinding& mapping,
    const RegisterBindings& registers, std::size_t target = 0) {
    auto fail = [](std::string message) -> Result<metaarch::Encoded> {
        return Diagnostic{"JUN-E500", {}, std::move(message), 0, 0};
    };
    if (mapping.parameters.size() != mapping.operand_order.size())
        return fail("form parameter and operand mapping lengths differ");
    std::vector<metaarch::Operand> operands;
    for (int slot : mapping.operand_order) {
        Operand arg;
        if (slot == -1) {
            if (instruction.result.empty()) return fail("mapping requests an absent instruction result");
            arg = {Operand::Kind::reg, instruction.result, 0};
        } else if (slot >= 0 && static_cast<std::size_t>(slot) < instruction.operands.size()) {
            arg = instruction.operands[static_cast<std::size_t>(slot)];
        } else return fail("instruction operand mapping is out of bounds");
        if (arg.kind == Operand::Kind::immediate) operands.emplace_back(arg.value);
        else if (arg.kind == Operand::Kind::reg) {
            auto found = registers.find(arg.name);
            if (found == registers.end()) return fail("no physical register assignment for '" + arg.name + "'");
            operands.emplace_back(found->second);
        } else return fail("labels and symbols require explicit relocation/ABI lowering");
    }
    auto encoded = spec.encode_form(mapping.form, mapping.parameters, operands, target);
    if (!encoded) {
        std::string message;
        for (const auto& d : encoded.error()->diagnostics) { if (!message.empty()) message += '\n'; message += d.message; }
        return fail(message.empty() ? "MetaArch form encoding failed" : message);
    }
    return std::move(*encoded.get());
}

// The branch relocation is explicit, just like FormBinding. This does NOT move
// arguments/results, align the stack, or save caller-clobbered registers. Those
// ABI operations must already have been supplied by the backend/caller.
struct ExternalCallBinding {
    std::string encoder, parameter = "sym";
    std::int64_t addend = 0;
    std::vector<mair::Binding> inputs; // any additional encoder parameters
};
inline Result<mair::Image> encode_external_call(const MachineModule& module,
    const Instruction& instruction, const ExternalCallBinding& binding, const mair::Target& target) {
    auto fail = [](std::string message) -> Result<mair::Image> {
        return Diagnostic{"JUN-E500", {}, std::move(message), 0, 0};
    };
    if (instruction.opcode != "CALL" || !instruction.barrier || !instruction.external ||
        *instruction.external >= module.externals.size() || instruction.operands.empty())
        return fail("expected a declared external CALL");
    const auto& imported = module.externals[*instruction.external];
    if (instruction.operands[0].kind != Operand::Kind::symbol || instruction.operands[0].name != imported.symbol ||
        instruction.operands.size() - 1 < imported.params.size() ||
        (!imported.variadic && instruction.operands.size() - 1 != imported.params.size()))
        return fail("external CALL disagrees with its declaration");
    try {
        mair::Program program;
        for (const auto& f : module.externals) program.extern_function(f.name, f.library, f.symbol);
        auto inputs = binding.inputs;
        inputs.push_back({binding.parameter, mair::symbol(imported.name, binding.addend)});
        program.encode(binding.encoder, inputs);
        return mair::assemble(program, target);
    } catch (const mair::Error& error) { return fail(error.what()); }
}
} // namespace juniper
