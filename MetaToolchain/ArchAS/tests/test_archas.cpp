#include "../ArchAS.hpp"
#include <iostream>

namespace {
size_t checks = 0, failures = 0;
void check(bool value, const char *test) {
    ++checks;
    if (!value) { ++failures; std::cerr << "FAIL: " << test << '\n'; }
}
template<class F> void rejects(F action, const std::string &contains) {
    try { action(); check(false, contains.c_str()); }
    catch (const archas::Error &e) { check(std::string(e.what()).find(contains) != std::string::npos, e.what()); }
}
} // namespace
int main(int argc, char **argv) {
    try {
        if (argc != 2) return 2;
        std::filesystem::path root = argv[1];
        auto amd64 = archas::load_target("amd64", root / "MetaArchDB");
        archas::Options options;
        options.backend = archas::Backend::builtin; options.format = archas::Format::hex;
        archas::Assembler assembler(amd64, options);
        auto hex = [&](std::string_view source) { return assembler.compile(source, "unit.mair").data; };
        check(hex("encode ret") == "c3\n", "MetaArch encoder");
        check(hex(".encode movimm, dst=%rax, imm=$42\nencode ret") == "48b82a00000000000000c3\n", "named physical register");
        check(hex(".byte 1 + 2 * 3, (4 | 8) << 1, ~0 & 255\n") == "0718ff\n", "absolute expressions");
        check(hex(".quad -9223372036854775808\n") == "0000000000000080\n", "signed minimum");
        check(hex(".byte 077, 0b11\n") == "3f03\n", "GAS octal and binary");
        check(hex(".byte -077\n") == "c1\n", "negative GAS octal");
        check(hex(".byte 'A, ';, '#, '\\n, 'Z'\n") == "413b230a5a\n", "GAS character constants");
        check(hex("/* block\ncomment */ .byte 1; .byte 2 # trailing\n") == "0102\n", "comments and separators");
        check(hex(".ascii \";#//\", \"x\"\n.asciz \"y\"\n") == "3b232f2f787900\n", "quoted delimiters");
        check(hex(".set N, 1<<2\n.if N==4\n.byte N\n.else\n.byte 0\n.endif\n") == "04\n", "conditionals");
        check(hex(".if 0\n.if unknown\n.byte 1\n.endif\n.elseif 1\n.byte 2\n.endif\n") == "02\n", "inactive branches");
        check(hex(".macro data n:req, value=7\n.rept \\n\n.byte \\value\n.endr\n.endm\ndata 3\n") == "070707\n", "nested macros/defaults");
        check(hex(".macro data a:req, b=1\n.byte \\a, \\b\n.endm\ndata b=4, a=2\n") == "0204\n", "keyword macros");
        check(hex(".macro m a=2,b=3\n.byte \\a,\\b\n.endm\nm ,7\nm 8,\n") == "02070803\n", "omitted macro arguments");
        check(hex(".macro m x:req\n.byte \\x\n.endm\nm (1==1)\n") == "01\n", "macro comparison argument");
        check(hex(".macro m x\n.ifb \\x\n.byte 1\n.else\n.ifc \\x,foo\n.byte 2\n.endif\n.endif\n.endm\nm\nm foo\n") == "0102\n", "GAS string conditionals");
        check(hex(".ifeq 0\n.byte 1\n.endif\n.ifgt 2\n.byte 2\n.endif\n.ifeqs \"a\",\"a\"\n.byte 3\n.endif\n") == "010203\n", "GAS comparison conditionals");
        check(hex(".macro data values:vararg\n.byte \\values\n.endm\ndata 1,2,3\n") == "010203\n", "variadic macro");
        check(hex(".macro m\n.Lx\\@:\n.byte 1\n.exitm\n.byte 2\n.endm\nm\nm\n") == "0101\n", "macro-local labels and exit");
        check(hex(".macro m\n.rept 3\n.byte 1\n.exitm\n.endr\n.byte 2\n.endm\nm\n.byte 3") == "0103\n", "exitm unwinds repetitions");
        check(hex(".macro m,n\n.byte \\n\n.endm\nm 7") == "07\n", "comma-separated macro declaration");
        check(hex(".irp n, 1, 2, 3\n.byte \\n\n.endr\n.irpc n, \"45\"\n.byte \\n\n.endr\n") == "0102030405\n", "irp/irpc");
        check(hex(".byte 1\n.balign 4\n.space 2,255\n") == "01000000ffff\n", "padding");
        check(hex("first: .byte 1\nsecond: .byte 2\n") == "0102\n", "inline labels");
        rejects([&] { hex(".encode missing"); }, "unknown encoder");
        rejects([&] { hex(".byte 256"); }, "does not fit");
        rejects([&] { hex(".quad 9223372036854775807+1"); }, "overflow");
        rejects([&] { hex(".quad -9223372036854775808*-1"); }, "overflow");
        rejects([&] { hex(".byte 1/0"); }, "division by zero");
        rejects([&] { hex(".byte 1<<64"); }, "shift count");
        rejects([&] { hex(".byte unknown"); }, "absolute constant");
        rejects([&] { hex(".byte (1+2"); }, "unterminated");
        rejects([&] { hex(".if 1\n.byte 1"); }, "unterminated .if");
        rejects([&] { hex(".else"); }, "unexpected");
        rejects([&] { hex(".macro m\n.byte 1"); }, "unterminated .macro");
        rejects([&] { hex(".macro m a:req\n.endm\nm"); }, "missing required");
        rejects([&] { hex(".macro m a\n.endm\nm a=1,a=2"); }, "duplicate macro");
        rejects([&] { hex(".macro m\nm\n.endm\nm"); }, "depth exceeded");
        rejects([&] { hex(".rept -1\n.endr"); }, "count out of range");
        rejects([&] { hex(".text\n.byte 1\n.data\n.byte 2"); }, "one section");
        rejects([&] { hex("encode call, sym=@external-4"); }, "unresolved relocations");
        rejects([&] { hex("mov eax,42"); }, "builtin backend");
        rejects([&] { hex(std::string(".byte 1\0\n", 9)); }, "control character");
        try { hex("# comment\n.encode missing\n"); check(false, "positioned error"); }
        catch (const archas::Error &e) { check(e.location.file == "unit.mair" && e.location.line == 2, "positioned error"); }

        assembler.directive("twice", [](std::string_view args, const archas::DirectiveContext &ctx) {
            if (ctx.target.name() != "amd64") throw archas::Error("wrong callback target");
            return ".byte " + std::string(args) + "," + std::string(args);
        });
        check(hex(".twice 9") == "0909\n", "C++ directive");
        check(hex(".macro m v\n.twice \\v\n.endm\nm 5") == "0505\n", "directive in macro");
        rejects([&] { assembler.directive("twice", {}); }, "duplicate");
        rejects([&] { assembler.directive("byte", [](auto, auto &) { return ""; }); }, "reserved");
        auto bounded = options; bounded.limits.statements = 10;
        rejects([&] { archas::Assembler(amd64, bounded).compile(".rept 100\n.endr"); }, "count out of range");
        bounded.limits.statements = 1;
        check(archas::Assembler(amd64, bounded).compile("encode ret\n").data == "c3\n", "exact statement budget");
        bounded = options; bounded.limits.bytes = 32;
        rejects([&] { archas::Assembler(amd64, bounded).compile(".space 33"); }, "invalid space");

        for (const auto &[name, expected] : std::map<std::string, std::string>{{"amd64", "c3\n"}, {"aarch64", "c0035fd6\n"}, {"riscv64", "67800000\n"}}) {
            archas::Assembler a(archas::load_target(name, root / "MetaArchDB"), options);
            check(a.compile("encode ret").data == expected, name.c_str());
        }
        auto riscv = archas::load_target("riscv64", root / "MetaArchDB");
        check(archas::Assembler(riscv, options).compile("form 11, rd=x10, rs1=x10, imm=-1").data == "1305f5ff\n", "checked RISC-V form");
        rejects([&] { archas::Assembler(riscv, options).compile("form 11, rd=x10, rs1=x10, imm=2048"); }, "operand");
        auto json = options; json.format = archas::Format::json;
        check(archas::Assembler(amd64, json).compile("encode call, sym=@callee-4").data.find("\"symbol\":\"callee\"") != std::string::npos, "relocation image");
        auto gas = options; gas.format = archas::Format::assembly;
        check(archas::Assembler(amd64, gas).compile("encode call, sym=@callee-4").data.find(".reloc .-5+1, R_X86_64_PLT32, callee-4") != std::string::npos, "RELA lowering");
        check(archas::load_target("x86_64", root / "MetaArchDB").name() == "amd64", "target alias");
        check(archas::Assembler(archas::load_target("amd64", root / "ArchDB"), options).compile("encode ret").data == "c3\n", "Dsymspec loading");

        if (archas::has_lua()) {
            check(hex("{% for i=1,4 do %}\n.byte {{i*i}}\n{% end %}") == "01040910\n", "Lua control-flow templates");
            check(hex(".byte {{ ({value=7}).value }}\n") == "07\n", "Lua table expression");
            check(hex(".ascii {{string.format('%q', '}}')}}\n") == "7d7d\n", "quoted template delimiter");
            check(hex("{% domas.emit('.byte 8') %}\n") == "08\n", "Lua emit");
            check(hex("{% domas.directive('lbyte', function(a,c) assert(c.line==2); return '.byte '..a end) %}\n.lbyte 12") == "0c\n", "Lua directive context");
            check(hex(".byte {{domas.register('rax').width}}\n") == "40\n", "Lua register metadata");
            rejects([&] { hex("{{ unknown + 1 }}"); }, "Lua:");
            rejects([&] { hex("{{ 1"); }, "unterminated");
            rejects([&] { hex("{% domas.directive('byte', function() end) %}"); }, "reserved");
            auto limited = options; limited.limits.lua_instructions = 1000;
            rejects([&] { archas::Assembler(amd64, limited).compile("{% while true do end %}"); }, "instruction limit");
            limited = options; limited.limits.lua_memory = 128 * 1024;
            rejects([&] { archas::Assembler(amd64, limited).compile("{{string.rep('x', 1000000)}}"); }, "memory");
            check(hex("{% count=(count or 0)+1 %}.byte {{count}}") == "01\n" &&
                  hex("{% count=(count or 0)+1 %}.byte {{count}}") == "01\n", "fresh Lua translation units");
        } else rejects([&] { hex("{{1}}"); }, "disabled");
    } catch (const std::exception &e) { ++failures; std::cerr << "unexpected: " << e.what() << '\n'; }
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
