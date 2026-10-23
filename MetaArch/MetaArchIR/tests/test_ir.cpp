#include "../MetaArchIR.hpp"
#include <iostream>

namespace {
size_t checks = 0;
void check(bool value, const char *message) {
    ++checks; if (!value) throw std::runtime_error(message);
}
template<class F> void fails(F f, std::string_view part) {
    ++checks;
    try { f(); } catch (const mair::Error &e) {
        if (std::string_view(e.what()).find(part) != std::string_view::npos) return;
        throw std::runtime_error(std::string("unexpected error: ") + e.what());
    }
    throw std::runtime_error("expected failure: " + std::string(part));
}
}
int main(int argc, char **argv) {
    try {
        if (argc != 2) return 2;
        auto db = std::filesystem::path(argv[1]) / "MetaArchDB";
        auto amd = mair::target("amd64", db), arm = mair::target("aarch64", db), rv = mair::target("riscv64", db);
        check(amd.supports("jge") && amd.supports("movq"), "native metadata and suffixes");
        check(!amd.supports("movqq") && !amd.supports("jeq"), "reject repeated and conditional suffixes");
        check(!rv.supports("movq"), "target separation");
        check(rv.forms("addi") == std::vector<size_t>{11}, "form lookup");
        check(rv.parameters("addi").size() == 3, "encoder parameter lookup");
        check(rv.register_info("x10").enc == 10, "register lookup");
        fails([&] { rv.parameters("missing"); }, "unknown encoder");
        fails([&] { rv.register_info("missing"); }, "unknown register");
        mair::Program ret; ret.encode("ret");
        check(mair::assemble(ret, amd).flat() == std::vector<uint8_t>{0xc3}, "x86 ret");
        check(mair::assemble(ret, arm).flat() == std::vector<uint8_t>({0xc0, 0x03, 0x5f, 0xd6}), "aarch64 ret");
        check(mair::assemble(ret, rv).flat() == std::vector<uint8_t>({0x67, 0x80, 0, 0}), "riscv ret");
        auto parsed = mair::parse("\r\n// comment\r\nform 11, rd=x10, rs1=x10, imm=-1\r\n", "test.mair");
        check(mair::assemble(parsed, rv).flat() == std::vector<uint8_t>({0x13, 0x05, 0xf5, 0xff}), "independent addi anchor");
        check(parsed.statements()[0].location.line == 3, "source position");
        for (auto n : {-2048, -1, 0, 1, 2047}) {
            mair::Program p; p.form(11, {{"rd", std::string("x10")}, {"rs1", std::string("x10")}, {"imm", int64_t(n)}});
            auto bytes = mair::assemble(p, rv).flat();
            uint32_t expected = (uint32_t(n) & 4095u) * 1048576u + 0x50513u;
            check(bytes == std::vector<uint8_t>({uint8_t(expected), uint8_t(expected >> 8), uint8_t(expected >> 16), uint8_t(expected >> 24)}), "addi boundaries");
        }
        fails([&] { mair::assemble(mair::parse("form 11, rd=x10, rs1=x10, imm=2048"), rv); }, "bounds");
        fails([&] { mair::assemble(mair::parse("form 11, rd=f0, rs1=x10, imm=1"), rv); }, "register class");
        fails([&] { mair::assemble(mair::parse("form 999999"), rv); }, "form index");
        fails([&] { mair::assemble(mair::parse("encode addi, rd=missing, rs1=x10, imm=1"), rv); }, "unknown register");
        fails([&] { mair::assemble(mair::parse("encode addi, rd=x10"), rv); }, "not bound");
        fails([&] { mair::assemble(mair::parse("encode nope"), rv); }, "encoder");
        fails([&] { mair::assemble(mair::parse("ret"), rv); }, "native assembly");
        fails([&] { mair::render(mair::parse("not_an_instruction"), rv); }, "not declared");
        auto call = mair::assemble(mair::parse("byte 0\nencode call, sym=@external-4"), amd);
        check(call.relocations.size() == 1, "relocation retained");
        const auto &rel = call.relocations.front();
        check(rel.offset == 2 && rel.symbol == "external" && rel.value == -4 && rel.bit_width == 32, "relocation metadata");
        fails([&] { (void)call.flat(); }, "unresolved relocations");
        fails([&] { mair::render(mair::parse("encode call, sym=@external"), amd); }, "relocatable");
        fails([&] { mair::assemble(mair::parse("encode addi, rd=x10, rs1=x10, imm=@symbol"), rv); }, "not an emitted relocation");
        auto mixed_spec = metaarch::Spec::parse_dsl(R"(architecture {
            schema = 1;
            target { arch = mixed; addr_size = 32; byte_order = little; obj_format = ELF32; }
            reloc R { width = 16; offset = 8; }
            encoder e { body = "EMIT32BE 0\nEMIT_RELOCATION sym, R\n"; }
        })");
        check(bool(mixed_spec), "mixed-endian fixture loads");
        mair::Target mixed(std::move(*mixed_spec.get()), "mixed");
        auto mixed_image = mair::assemble(mair::parse("encode e, sym=@label"), mixed);
        check(mixed_image.relocations[0].endian == DSYM_ENDIAN_BIG &&
              mixed_image.relocations[0].bit_offset == 8 && mixed_image.relocations[0].bit_width == 16,
              "relocation actual field byte order and bit range");
        auto layout = mair::assemble(mair::parse("global a\nsection .data\na:\nbyte 1\nalign 4\nword 0x1234\nsection .text\nencode ret\nsection .data\nbyte 2"), amd);
        check(layout.sections.size() == 2 && layout.sections[0].alignment == 4, "section reuse/alignment");
        check(layout.sections[0].bytes == std::vector<uint8_t>({1, 0, 0, 0, 0x34, 0x12, 2}), "section layout");
        check(layout.symbols[0].offset == 0 && layout.symbols[0].global, "symbol layout");
        fails([&] { (void)layout.flat(); }, "one section");
        auto s390 = mair::target("s390x", db);
        check(mair::assemble(mair::parse("word 0x1234"), s390).flat() == std::vector<uint8_t>({0x12, 0x34}), "big endian data");
        fails([&] { mair::assemble(mair::parse("byte 256"), amd); }, "does not fit");
        fails([&] { mair::assemble(mair::parse("global missing"), amd); }, "not defined");
        std::string all;
        for (unsigned i = 0; i < 256; ++i) all += char(i);
        check(mair::unquote(mair::quote(all)) == all, "all byte escapes");
        auto strings = mair::parse("ascii \"a\\n\\000\\\\\" // ignored\n");
        check(strings.statements()[0].name == std::string("a\n\0\\", 4), "string decoding");
        auto native = mair::parse("add x0, x1, #42\nmov 8(%rax,%rcx,4), %rdx");
        check(native.statements()[0].args[2] == "#42", "ARM immediate marker");
        check(native.statements()[1].args.size() == 2, "nested operands");
        check(mair::parse("# comment\n// comment").empty(), "empty source");
        for (const auto *source : {"align -1", "align 4junk", "align 4294967297", "align 3", "ascii \"\\q\"", "ascii \"\\777\"", "ascii \"oops",
                                  "mov r0,", "mov ,r0", "mov ([)]", "encode ret, x=1, x=2", "a:\na:", "byte 9223372036854775808", "byte 0x", "ret; nop"}) {
            ++checks;
            try { mair::parse(source, "invalid.mair"); throw std::runtime_error("accepted invalid source: " + std::string(source)); }
            catch (const mair::Error &e) { check(e.location.line > 0 && e.location.file == "invalid.mair", "positioned error"); }
        }
        fails([] { mair::Program p; p.ins("mov", "a\nret"); }, "control character");
        fails([] { mair::Program p; p.comment("x\nret"); }, "control character");
        fails([] { mair::render(mair::Program{}, mair::Target{}); }, "not loaded");
        fails([&] { mair::target("..", db); }, "invalid target");
        fails([&] { mair::target("amd64", db, 999); }, "target index");
        mair::Limits limits; limits.source_bytes = 1;
        fails([&] { mair::parse("ret", "limit", limits); }, "source byte limit");
        limits = {}; limits.statements = 0;
        fails([&] { mair::parse("ret", "limit", limits); }, "statement limit");
        limits = {}; limits.nesting = 1;
        fails([&] { mair::parse("mov ((x))", "limit", limits); }, "nesting limit");
        limits = {}; limits.output_bytes = 0;
        fails([&] { mair::assemble(ret, amd, limits); }, "output byte limit");
        fails([&] { mair::render(ret, amd, limits); }, "output byte limit");
        auto names = mair::targets(db);
        check(names.size() == 28 && std::is_sorted(names.begin(), names.end()), "database enumeration");
        for (const auto &name : names) check(mair::target(name, db).spec().get()->mnemonic_count > 0, "load database via API");
        std::cout << checks << " MetaArchIR checks passed\n";
        return 0;
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
