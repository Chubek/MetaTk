#include "metachain/ASMtk.hpp"
#include <cstdlib>
#include <iostream>

#define CHECK(...) do { if (!(__VA_ARGS__)) { std::cerr << __LINE__ << ": " #__VA_ARGS__ "\n"; std::exit(1); } } while (false)

static void encodings(const asmtk::Architecture& architecture) {
    asmtk::Assembler a(architecture);
    const auto bytes = [&](std::string_view source, std::vector<std::uint8_t> expected) {
        auto result = a.assemble(source);
        if (!result) for (const auto& d : result.diagnostics) std::cerr << source << ": " << d.message << '\n';
        CHECK(result && result.bytes == expected);
    };
    bytes("nop\nret\nsyscall\nint3\nud2\ncqo\nleave\n", {0x90,0xc3,0x0f,0x05,0xcc,0x0f,0x0b,0x48,0x99,0xc9});
    bytes("mov r9, r10\nimul r9, r10\n", {0x4d,0x89,0xd1,0x4d,0x0f,0xaf,0xca});
    bytes("mov R9D, R10D\nMoV SPL, AL\nmov al, spl\n", {0x45,0x89,0xd1,0x40,0x88,0xc4,0x40,0x88,0xe0});
    bytes("mov r8b, 255\nmov r9d, 0x12345678\n", {0x41,0xb0,0xff,0x41,0xb9,0x78,0x56,0x34,0x12});
    bytes("push r15\npop r8\ncall r9\njmp r10\n", {0x41,0x57,0x41,0x58,0x41,0xff,0xd1,0x41,0xff,0xe2});
    bytes("inc r8\ndec r9d\nneg r10\nnot r11d\ndiv r12\nidiv r13\n",
        {0x49,0xff,0xc0,0x41,0xff,0xc9,0x49,0xf7,0xda,0x41,0xf7,0xd3,0x49,0xf7,0xf4,0x49,0xf7,0xfd});
    bytes("shl r8, CL\nshr r9d, cl\nsar r10, 63\n", {0x49,0xd3,0xe0,0x41,0xd3,0xe9,0x49,0xc1,0xfa,0x3f});
    bytes("add r9, -2147483648\nsub r10d, 2147483647\n",
        {0x49,0x81,0xc1,0,0,0,0x80,0x41,0x81,0xea,0xff,0xff,0xff,0x7f});
    bytes("mov r8, [r12]\nmov [r13-4], r9\nlea r10d, [r15+8]\n",
        {0x4d,0x8b,0x84,0x24,0,0,0,0,0x4d,0x89,0x8d,0xfc,0xff,0xff,0xff,0x45,0x8d,0x97,8,0,0,0});
    bytes("mov spl, [r8]\nmov [r12], al\n", {0x41,0x8a,0xa0,0,0,0,0,0x41,0x88,0x84,0x24,0,0,0,0});
    bytes("jmp end\ncall end\nend: ret\n", {0xe9,5,0,0,0,0xe8,0,0,0,0,0xc3});
    bytes("lea r8, [rel end]\nend: ret\n", {0x4c,0x8d,0x05,0,0,0,0,0xc3});
    const std::string_view jumps[] = {"jo","jno","jb","jae","je","jne","jbe","ja","js","jns","jp","jnp","jl","jge","jle","jg"};
    for (unsigned i = 0; i < std::size(jumps); ++i)
        bytes(std::string(jumps[i]) + " end\nend: ret\n", {0x0f,static_cast<std::uint8_t>(0x80+i),0,0,0,0,0xc3});
    bytes("jc end\njnc end\njz end\njnz end\nend: ret\n",
        {0x0f,0x82,18,0,0,0,0x0f,0x83,12,0,0,0,0x0f,0x84,6,0,0,0,0x0f,0x85,0,0,0,0,0xc3});
    for (auto source : {"nop rax", "wat", "mov rax, eax", "mov ax, bx", "mov ah, al",
                        "mov r16, rax", "mov xmm0, xmm1", "lea al, [rax]", "push eax", "call eax",
                        "add al, bl", "test rax, 1", "shl eax, 32", "shr rax, -1", "sar rax, dl",
                        "add rax, 2147483648", "mov al, 256", "mov al, -129", "mov rax, [eax]",
                        "mov rax, [r8+2147483648]", "jmp missing"}) {
        auto bad = a.assemble(std::string("nop\n") + source);
        CHECK(!bad && bad.bytes.empty() && !bad.diagnostics.empty() && bad.diagnostics[0].location.line == 2);
    }
    const auto object = a.assemble_object(".extern f\ncall f\njmp f\nmov rax, f\n");
    CHECK(object && object.relocations.size() == 3);
    CHECK(object.relocations[0].type == 4 && object.relocations[1].type == 2 && object.relocations[2].type == 1);
    CHECK(object.relocations[0].addend == -4 && object.relocations[1].addend == -4);
}

#if defined(METACHAIN_ENABLE_SYMSPEC)
static asmtk::Architecture from_value(symspec::Json value) {
    auto spec = symspec::Spec::from_value(std::move(value)); CHECK(spec);
    return asmtk::Architecture(std::move(*spec));
}
static void loaded_tests(const char* path) {
    auto spec = symspec::Spec::load(path); CHECK(spec);
    const auto original = spec->json();
    asmtk::Architecture architecture(std::move(*spec));
    CHECK(architecture.valid() && architecture.name() == "amd64" && architecture.get());
    encodings(architecture);
    auto retained = [&] { auto local = from_value(original); return asmtk::Architecture(local); }();
    encodings(retained);
    // Both names and opcodes must come from the loaded bank, not a fallback.
    auto custom = original;
    custom.as_object().at("registers").as_array()[2].as_object()["name"] = "acc";
    custom.as_object().at("registers").as_array()[3].as_object()["name"] = "count";
    for (auto& e : custom.as_object().at("asmtk").as_object().at("encodings").as_array())
        if (symspec::text(symspec::field(e, "form")) == "ShiftCL") e.as_object()["implicit_register"] = "count";
    custom.as_object().at("asmtk").as_object().at("encodings").as_array()[0].as_object()["opcode"] = boost::json::array{0xcc};
    asmtk::Assembler changed(from_value(std::move(custom)));
    CHECK(changed.architecture().valid());
    CHECK(changed.assemble("nop\nmov ACC, 1\n").bytes == std::vector<std::uint8_t>({0xcc,0x48,0xb8,1,0,0,0,0,0,0,0}));
    CHECK(!changed.assemble("mov rax, 1"));
    CHECK(changed.assemble("shl acc, COUNT").bytes == std::vector<std::uint8_t>({0x48,0xd3,0xe0}));
    // Failed loads are transactional and leave the previous bank usable.
    CHECK(changed.load_architecture(path));
    CHECK(!changed.load_architecture(std::string(path) + ".missing"));
    CHECK(changed.assemble("nop").bytes == std::vector<std::uint8_t>{0x90});
    auto invalid_document = original;
    invalid_document.as_object().erase("asmtk");
    symspec::LoadOptions invalid_load;
    invalid_load.format = symspec::InputFormat::lua_dsl;
    invalid_load.evaluator = [&](std::string_view, const symspec::Path&) -> symspec::Result<std::string> {
        return boost::json::serialize(invalid_document);
    };
    CHECK(!changed.load_architecture(path, invalid_load));
    CHECK(changed.assemble("nop").bytes == std::vector<std::uint8_t>{0x90});
    for (unsigned problem = 0; problem < 21; ++problem) {
        auto broken = original;
        auto& extension = broken.as_object().at("asmtk").as_object();
        auto& e = extension.at("encodings").as_array()[0].as_object();
        switch (problem) {
        case 0: broken.as_object().erase("asmtk"); break;
        case 1: extension["schema"] = 2; break;
        case 2: e["opcode"] = boost::json::array{256}; break;
        case 3: e["opcode"] = boost::json::array{}; break;
        case 4: e["form"] = "ArbitraryCode"; break;
        case 5: e["widths"] = boost::json::array{16}; break;
        case 6: e["group"] = 8; break;
        case 7: e["rex_w"] = "true"; break;
        case 8: e["relocation"] = 4; break;
        case 9: { auto duplicate = extension.at("encodings").as_array()[0]; extension.at("encodings").as_array().push_back(duplicate); break; }
        case 10: broken.as_object().at("registers").as_array()[0].as_object()["enc"] = -1; break;
        case 11: broken.as_object().at("registers").as_array()[0].as_object()["width"] = 0; break;
        case 12: broken.as_object().at("registers").as_array()[0].as_object()["sub_lo"] = "bad"; break;
        case 13: e["opcode"] = boost::json::array{1,2,3,4}; break;
        case 14: e["widths"] = boost::json::array{8,8}; break;
        case 15: e["reg_operand"] = 2; break;
        case 16: e["implicit_register"] = "cl"; break;
        case 17: broken.as_object().at("targets").as_array()[0].as_object()["byte_order"] = "big"; break;
        case 18: broken.as_object().at("targets").as_array()[0].as_object()["addr_size"] = 32; break;
        case 19:
            for (auto& entry : extension.at("encodings").as_array())
                if (symspec::text(symspec::field(entry, "form")) == "ShiftCL") entry.as_object()["implicit_register"] = "missing";
            break;
        case 20:
            for (auto& entry : extension.at("encodings").as_array())
                if (symspec::text(symspec::field(entry, "form")) == "MoveImmediate") entry.as_object()["opcode"] = boost::json::array{0xb9};
            break;
        }
        auto invalid = from_value(std::move(broken));
        CHECK(!invalid.valid() && !invalid.error().empty());
        auto bad = asmtk::Assembler(invalid).assemble(".byte 1");
        CHECK(!bad && bad.bytes.empty());
    }
    auto other = original;
    other.as_object().at("targets").as_array()[0].as_object()["arch"] = "aarch64";
    CHECK(!from_value(std::move(other)).valid());
}
#endif
int main(int argc, char** argv) {
    encodings(asmtk::Architecture{});
#if defined(METACHAIN_ENABLE_SYMSPEC)
    CHECK(argc == 2); loaded_tests(argv[1]);
#else
    (void)argc; (void)argv;
#endif
    std::cout << "Arch-backed assembler encodings and validation passed\n";
}
