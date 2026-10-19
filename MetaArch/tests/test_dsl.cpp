#include "MetaArch.hpp"
#include <cstdlib>
#include <iostream>

#define CHECK(expr) do { if (!(expr)) { std::cerr << __LINE__ << ": " #expr "\n"; std::exit(1); } } while (0)
static void explain(const metaarch::Error *e) {
    if (e) for (const auto &d : e->diagnostics)
        std::cerr << d.file << ':' << d.line << ':' << d.column << ' ' << d.message << '\n';
}
int main(int argc, char **argv) {
    CHECK(argc == 2);
    std::string root = argv[1];
    auto spec = metaarch::Spec::load(root + "/examples/toy.march");
    explain(spec.error()); CHECK(spec);
    CHECK(spec.get()->find_register("r1")->enc == 1);
    CHECK(!spec.get()->find_register("unknown"));
    auto out = spec.get()->assemble("mov r1, -1\nmov imm=42, dst=r0\n");
    explain(out.error()); CHECK(out);
    CHECK(out.get()->bytes == std::vector<uint8_t>({0x11, 0xff, 0x10, 42}));
    CHECK(spec.get()->assemble("# empty\n ; comment\n").get()->bytes.empty());
    CHECK(!spec.get()->assemble("mov r1, 128"));
    CHECK(!spec.get()->assemble("mov r2, 1"));
    CHECK(!spec.get()->assemble("mov dst=r0, dst=r1"));
    CHECK(!spec.get()->assemble("mov dst=r0, imm=1, extra=0"));
    CHECK(!spec.get()->assemble("mov dst=r0, 1"));
    CHECK(!spec.get()->assemble("mov r0, 1,"));
    CHECK(!spec.get()->assemble("mov r0 1"));
    CHECK(!spec.get()->assemble("mov r0, 1 trailing"));
    CHECK(!spec.get()->assemble("mov r0, 9223372036854775808"));
    CHECK(!spec.get()->assemble("mov r0, 0x"));
    CHECK(!spec.get()->assemble("label:\nmov r0, 1"));
    CHECK(!spec.get()->assemble("mov r0, [r1]"));
    CHECK(!spec.get()->assemble("mov r0, 1+2"));
    CHECK(!spec.get()->assemble("mov r0, r1"));
    CHECK(!spec.get()->assemble("mov 0, 1"));
    std::vector<std::string> form_params = {"dst", "imm"};
    std::vector<metaarch::Operand> form_args = {std::string("r1"), int64_t(-1)};
    auto form_encoded = spec.get()->encode_form(0, form_params, form_args); CHECK(form_encoded);
    CHECK(form_encoded.get()->bytes == std::vector<uint8_t>({0x11, 0xff}));
    CHECK(!spec.get()->encode_form(1, form_params, form_args));
    auto bad = spec.get()->assemble("# comment\nmov r0, 1\nnope\n", "test.asm");
    CHECK(!bad); CHECK(bad.error()->diagnostics[0].line == 3);
    CHECK(bad.error()->diagnostics[0].file == "test.asm");
    metaarch::AssembleOptions options;
    options.target = 1; CHECK(!spec.get()->assemble("", "test", options));
    options.target = 0; options.max_output_bytes = 1;
    CHECK(!spec.get()->assemble("mov r0, 1", "test", options));
    for (auto fmt : {DSYM_FORMAT_YAML, DSYM_FORMAT_JSON, DSYM_FORMAT_XML, DSYM_FORMAT_SEXPR}) {
        auto text = spec.get()->write(fmt); CHECK(text);
        auto copy = metaarch::Spec::parse(*text.get(), fmt); CHECK(copy);
        CHECK(copy.get()->get()->register_count == 2);
        CHECK(copy.get()->assemble("mov dst=r1, imm=-1").get()->bytes == std::vector<uint8_t>({0x11, 0xff}));
        // DSL-only assembly metadata is deliberately not exported to schema 1.
        CHECK(!copy.get()->assemble("mov r1, -1"));
    }
    auto rv = metaarch::Spec::load(root + "/ArchDB/riscv64.dsymspec.yaml"); CHECK(rv);
    auto add = rv.get()->assemble("addi rd=a0, rs1=a0, imm=1");
    explain(add.error()); CHECK(add);
    CHECK(add.get()->bytes == std::vector<uint8_t>({0x13, 0x05, 0x15, 0x00}));
    options = {};
    options.bindings["addi"] = {"rd", "rs1", "imm"};
    CHECK(rv.get()->assemble("addi a0, a0, 1", "test", options).get()->bytes == add.get()->bytes);
    CHECK(!rv.get()->assemble("addi f0, a0, 1", "test", options));
    CHECK(!rv.get()->assemble("addi a0, a0, 2048", "test", options));
    options.bindings["addi"] = {"rd", "rd", "imm"};
    CHECK(!rv.get()->assemble("addi rd=a0, rs1=a0, imm=1", "test", options));
    auto x86 = metaarch::Spec::load(root + "/ArchDB/x86.dsymspec.yaml"); CHECK(x86);
    CHECK(x86.get()->assemble("add dst=eax, src=ecx").get()->bytes == std::vector<uint8_t>({0x01, 0xc8}));
    CHECK(!metaarch::Spec::parse_dsl("architecture { schema = 1; schema = 1; }"));
    CHECK(!metaarch::Spec::parse_dsl("architecture { schema = 1 }"));
    CHECK(!metaarch::Spec::parse_dsl("architecture {} trailing"));
    CHECK(!metaarch::Spec::parse_dsl("architecture { schema = 1; targets = []; }"));
    CHECK(!metaarch::Spec::parse_dsl("architecture { schema = 1; targets = ["));
    CHECK(!metaarch::Spec::parse_dsl("architecture { schema = 1; x = \"\\q\"; }"));
    CHECK(!metaarch::Spec::parse_dsl("architecture { schema = 1; x = \"unterminated"));
    metaarch::ParseOptions limits;
    limits.max_source_bytes = 4;
    CHECK(!metaarch::Spec::parse_dsl("architecture {}", "test", limits));
    limits = {}; limits.max_depth = 2;
    CHECK(!metaarch::Spec::parse_dsl("architecture { x = [[[[[]]]]]; }", "test", limits));
    const std::string minimal = "architecture { schema = 1; target { arch = test; addr_size = 32; "
        "byte_order = little; obj_format = ELF32; } encoder e { body = \"EMIT8 x\\n\"; } ";
    CHECK(!metaarch::Spec::parse_dsl(minimal + "assembly = { e = [x, x]; }; }"));
    CHECK(!metaarch::Spec::parse_dsl(minimal + "assembly = { e = [bad]; }; }"));
    CHECK(!metaarch::Spec::parse_dsl(minimal + "assembly = { e = []; }; }"));
    CHECK(!metaarch::Spec::parse_dsl(minimal + "assembly = { missing = [x]; }; }"));
    auto signed_min = metaarch::Spec::parse_dsl(minimal + "assembly = { e = [x]; }; }"); CHECK(signed_min);
    CHECK(!signed_min.get()->assemble("e -9223372036854775808")); // parsed, but not an 8-bit output
    auto boundaries = signed_min.get()->assemble("e 0b10\ne 0o10\ne +0x10"); CHECK(boundaries);
    CHECK(boundaries.get()->bytes == std::vector<uint8_t>({2, 8, 16}));
    auto bounded = metaarch::Spec::parse_dsl(minimal +
        "mnemonic e {} form { mnemonic = e; encoder = e; operands = [{ kind = imm; imm_min = 0; imm_max = 10; }]; } "
        "assembly = { e = [x]; }; }");
    explain(bounded.error()); CHECK(bounded);
    CHECK(bounded.get()->assemble("e 10"));
    CHECK(!bounded.get()->assemble("e 11")); // form bounds, even without encoder REQUIRE
    CHECK(!bounded.get()->assemble("e x=11"));
    // All returned relocation strings and records survive spec destruction.
    metaarch::Encoded relocations;
    {
        auto reloc = metaarch::Spec::parse_dsl("architecture { schema = 1; target { arch = test; addr_size = 32; "
            "byte_order = little; obj_format = ELF32; } reloc R_TEST { width = 32; } "
            "encoder e { body = \"EMIT_IMM 4, sym\\nEMIT_RELOCATION sym, R_TEST\\n\"; } }");
        explain(reloc.error()); CHECK(reloc);
        auto result = reloc.get()->assemble("e sym=1\ne sym=2"); CHECK(result);
        relocations = std::move(*result.get());
    }
    CHECK(relocations.relocations.size() == 2);
    CHECK(relocations.relocations[1].offset == 4);
    CHECK(relocations.relocations[1].name == "R_TEST");
    CHECK(relocations.relocations[1].parameter == "sym");
    metaarch::Spec moved = std::move(*spec.get());
    CHECK(!spec.get()->write(DSYM_FORMAT_JSON));
    CHECK(moved.find_register("r0"));
    std::cout << "DSL, C++ API and assembler tests passed\n";
}
