#include <juniper/MetaArch.hpp>
#include <iostream>

int main() {
    auto spec = metaarch::Spec::parse_dsl(R"(architecture {
      schema = 1;
      target { arch = toy; addr_size = 64; byte_order = little; obj_format = ELF64; }
      register r0 { class = gpr; width = 64; enc = 0; }
      register r1 { class = gpr; width = 64; enc = 1; }
      mnemonic addi {}
      mnemonic call {}
      reloc R_CALL { width = 32; pcrel = true; }
      encoder call { max_bytes = 5; max_relocs = 1; body = "EMIT8 0xE8\nEMIT32LE 0\nEMIT_RELOCATION sym, R_CALL\n"; }
      encoder addi { body = "REQUIRE dst >= 0 && dst <= 1\nREQUIRE src >= 0 && src <= 1\nEMIT8 0x10 | dst\nEMIT8 src\nEMIT8 (imm & 255)\n"; }
      form { mnemonic = addi; encoder = addi; operands = [
        {kind = reg; class = gpr; width = 64;}, {kind = reg; class = gpr; width = 64;},
        {kind = imm; width = 8; signed = true; imm_min = -128; imm_max = 127;}
      ]; }
    })");
    if (!spec) {
        for (const auto& d : spec.error()->diagnostics) std::cerr << d.message << '\n';
        return 1;
    }
    juniper::Instruction i; i.opcode = "ADDI"; i.result = "$v0";
    i.operands = {{juniper::Operand::Kind::reg, "x", 0}, {juniper::Operand::Kind::immediate, {}, -1}};
    juniper::FormBinding mapping{0, {"dst", "src", "imm"}, {-1, 0, 1}};
    juniper::RegisterBindings registers{{"$v0", "r1"}, {"x", "r0"}};
    auto encoded = juniper::encode_instruction(*spec.get(), i, mapping, registers);
    if (!encoded) { std::cerr << juniper::format(*encoded.error()) << '\n'; return 1; }
    if (encoded.get()->bytes != std::vector<std::uint8_t>{0x11, 0x00, 0xff}) return 1;
    i.operands[1].value = 128;
    if (juniper::encode_instruction(*spec.get(), i, mapping, registers)) return 1;
    i.operands[1].value = 1;
    registers.erase("x");
    if (juniper::encode_instruction(*spec.get(), i, mapping, registers)) return 1;
    juniper::Compiler compiler;
    auto parsed = compiler.parse("extern fn magnitude(i64) -> i64 from \"c\" symbol \"labs\"; fn f(x){return magnitude(x);}");
    if (!parsed) return 1;
    auto selected = juniper::select(*parsed.get());
    if (!selected) return 1;
    auto scheduled = juniper::schedule(*selected.get());
    if (!scheduled) return 1;
    mair::Target target(std::move(*spec.get()), "toy");
    juniper::ExternalCallBinding call_mapping{"call", "sym", -4, {}};
    const auto& call = scheduled.get()->functions[0].instructions[0];
    auto foreign = juniper::encode_external_call(*scheduled.get(), call, call_mapping, target);
    if (!foreign) { std::cerr << juniper::format(*foreign.error()) << '\n'; return 1; }
    if (foreign.get()->imports.size() != 1 || foreign.get()->imports[0].library != "c" ||
        foreign.get()->relocations.size() != 1 || foreign.get()->relocations[0].symbol != "labs" ||
        foreign.get()->relocations[0].value != -4 || foreign.get()->relocations[0].offset != 1) return 1;
    if (juniper::encode_external_call(*scheduled.get(), scheduled.get()->functions[0].instructions.back(), call_mapping, target)) return 1;
    call_mapping.parameter = "missing";
    if (juniper::encode_external_call(*scheduled.get(), call, call_mapping, target)) return 1;
    std::cout << "checked MetaArch form encoding and failure propagation passed\n";
}
