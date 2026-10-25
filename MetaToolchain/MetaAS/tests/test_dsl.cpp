#include <MetaAS/MetaAS-DSL.hpp>
#include <filesystem>
#include <iostream>
#include <locale>
#include <source_location>
#include <type_traits>

metaas::dsl::Program other_program();
namespace {
size_t checks = 0;
void check(bool value, std::source_location where = std::source_location::current()) {
    ++checks;
    if (!value) throw std::runtime_error("check failed: " + std::to_string(checks) + " at line " + std::to_string(where.line()));
}
template<class E, class F> void rejects(F f, std::string_view message) {
    try { f(); } catch (const E &e) {
        check(std::string_view(e.what()).find(message) != std::string_view::npos); return;
    }
    throw std::runtime_error("expected failure: " + std::string(message));
}
struct Grouped : std::numpunct<char> {
    char do_thousands_sep() const override { return ','; }
    std::string do_grouping() const override { return "\3"; }
};
}
int main(int argc, char **argv) {
    using namespace metaas::dsl;
    static_assert(!std::is_copy_constructible_v<metaas::Assembler>);
    static_assert(!std::is_copy_constructible_v<metaas::Result>);
    static_assert(!std::is_constructible_v<Immediate, bool>);
    static_assert(!std::is_constructible_v<Immediate, double>);
    try {
        check(argc == 2);
        auto database = (std::filesystem::path(argv[1]) / "MetaArchDB").string();
        check(imm('A').value() == 65 && imm(char32_t(0x100)).value() == 256);
        check(imm(INT64_MIN).value() == INT64_MIN && imm(uint64_t(INT64_MAX)).value() == INT64_MAX);
        auto options = metaas::options(); options.target = "amd64";
        options.database = database.c_str(); options.backend = METAAS_BUILTIN; options.format = METAAS_HEX;
        metaas::Assembler assembler(options);
        auto p = Program{} | section(".text") | global("answer") | label("answer")
            | encode("movimm", {arg<"dst">(reg("rax")), arg<"imm">(imm(42))}) | encode("ret");
        check((p | compile(assembler)).text() == "48b82a00000000000000c3\n");
        check(p.size() == 5);
        check(other_program().compile(assembler).text() == "c3\n");
        auto stage = encode("ret");
        Program base;
        auto a = base | stage;
        auto b = base | stage;
        check(base.empty() && a.source() == b.source());
        auto moved = std::move(a);
        check(a.empty() && a.source().empty());
        a.encode("ret"); check(a.source() == moved.source());
        auto copy = p; copy.clear(); check(!p.empty() && copy.empty());
        copy = std::move(moved); check(moved.empty() && copy.size() == 1);
        std::string before(p.source());
        rejects<std::invalid_argument>([&] { p.label("answer"); }, "duplicate label");
        rejects<std::invalid_argument>([&] { p.encode("ret\n.byte 1"); }, "identifier");
        rejects<std::invalid_argument>([&] { p.encode("movimm", {{"dst", reg("rax")}, {"dst", imm(1)}}); }, "duplicate");
        rejects<std::invalid_argument>([&] { p.encode("ret", {{"bad;name", imm(1)}}); }, "identifier");
        rejects<std::invalid_argument>([&] { reg(std::string("rax\0junk", 8)); }, "identifier");
        rejects<std::invalid_argument>([&] { symbol("{{boom}}"); }, "identifier");
        rejects<std::invalid_argument>([&] { p.section("text"); }, "section");
        rejects<std::invalid_argument>([&] { p.align(3); }, "alignment");
        rejects<std::invalid_argument>([&] { p.form(0, {arg("sym", symbol("callee"))}); }, "symbolic");
        rejects<std::out_of_range>([&] { imm(UINT64_MAX); }, "64-bit");
        rejects<std::out_of_range>([&] { p.data(Width::byte, {256}); }, "width");
        rejects<std::out_of_range>([&] { p.data(Width::word, {-32769}); }, "width");
        rejects<std::invalid_argument>([&] { p.data(static_cast<Width>(3), {1}); }, "width");
        rejects<std::invalid_argument>([&] { p.data(Width::byte, {}); }, "empty");
        check(p.source() == before);
        Program bounded({12, 1}); bounded.encode("ret");
        rejects<std::length_error>([&] { bounded.label("x"); }, "statement");
        bounded.clear(); bounded.label("x"); check(bounded.size() == 1);
        Program small({3, 10}); small.label("x");
        rejects<std::length_error>([&] { small.encode("ret"); }, "byte");
        check(small.source() == "x:\n");
        Program no_room({2, 10});
        rejects<std::length_error>([&] { no_room.label("x"); }, "byte");
        rejects<std::length_error>([&] { no_room.label("x"); }, "byte");
        Program data;
        data.data(Width::byte, {-128, -1, 0, 1, 255}).data(Width::word, {-32768, 65535})
            .data(Width::long_, {INT32_MIN, UINT32_MAX}).data(Width::quad, {INT64_MIN, INT64_MAX});
        check(data.compile(assembler).text() == "80ff0001ff0080ffff00000080ffffffff0000000000000080ffffffffffffff7f\n");
        Program aligned; aligned.data(Width::byte, {1}).align(4).data(Width::byte, {2});
        check(aligned.compile(assembler).text() == "0100000002\n");
        Program literal;
        literal.ascii(std::string("{{1}}\0\xff", 7));
        literal.bytes({}); literal.ascii({});
        check(literal.size() == 1 && literal.compile(assembler).text() == "7b7b317d7d00ff\n");
        auto old = std::locale(); std::locale::global(std::locale(old, new Grouped));
        Program locale; locale.data(Width::quad, {1234567});
        std::locale::global(old);
        check(locale.source() == ".8byte 1234567\n");
        for (const auto &[target, expected] : std::vector<std::pair<const char *, const char *>>{
                 {"amd64", "c3\n"}, {"aarch64", "c0035fd6\n"}, {"riscv64", "67800000\n"}}) {
            options.target = target; metaas::Assembler as(options);
            check(other_program().compile(as).text() == expected);
        }
        options.target = "riscv64"; metaas::Assembler riscv(options);
        for (int n : {-2048, -1, 0, 1, 2047}) {
            auto form_program = Program{} | form(11, {arg("rd", reg("x10")), arg("rs1", reg("x10")), arg("imm", imm(n))});
            const char *expected = n == -2048 ? "13050580\n" : n == -1 ? "1305f5ff\n" : n == 0 ? "13050500\n" : n == 1 ? "13051500\n" : "1305f57f\n";
            check(form_program.compile(riscv).text() == expected);
        }
        Program bad; bad.form(11, {arg("rd", reg("x10")), arg("rs1", reg("x10")), arg("imm", imm(2048))});
        rejects<metaas::Error>([&] { bad.compile(riscv); }, "operand");
        bad.clear(); bad.encode("missing");
        try { bad.compile(assembler, "generated.cpp"); check(false); }
        catch (const metaas::Error &e) { check(e.detail.line == 1 && std::string(e.detail.file) == "generated.cpp"); }
        check(other_program().compile(assembler).text() == "c3\n");
        options.target = "amd64"; options.format = METAAS_JSON; metaas::Assembler json(options);
        Program relocation; relocation.encode("call", {arg("sym", symbol("callee", -4))});
        auto image = relocation.compile(json).text();
        check(image.find("\"symbol\":\"callee\"") != std::string::npos);
        check(image.find("\"value\":-4") != std::string::npos);
        rejects<metaas::Error>([&] { relocation.compile(assembler); }, "relocations");
        std::string nul("x\0junk", 6);
        rejects<std::invalid_argument>([&] { assembler.include(nul); }, "NUL");
        rejects<std::invalid_argument>([&] { assembler.lua(nul); }, "NUL");
        rejects<std::invalid_argument>([&] { assembler.plugin(nul); }, "NUL");
        rejects<std::invalid_argument>([&] { assembler.assembler_argument(nul); }, "NUL");
        rejects<std::invalid_argument>([&] { assembler.define(nul, 1); }, "NUL");
        rejects<std::invalid_argument>([&] { assembler.compile("encode ret", nul); }, "NUL");
        rejects<std::invalid_argument>([&] { assembler.directive(nul, [](auto, auto &) { return ""; }); }, "NUL");
        assembler.directive("one", [](auto, auto &) { return ".byte 1"; });
        auto owner = std::move(assembler);
        rejects<std::logic_error>([&] { assembler.compile(""); }, "moved-from");
        check(owner.compile(".one").text() == "01\n");
        auto token = std::make_shared<int>(1); std::weak_ptr<int> weak = token;
        json.directive("capture", [token](auto, auto &) { return ".byte 2"; }); token.reset();
        json = std::move(owner); check(weak.expired());
        check(json.compile(".one").text() == "01\n");
        auto result = [&] { options.format = METAAS_HEX; metaas::Assembler temporary(options); return other_program().compile(temporary); }();
        check(result.text() == "c3\n");
#if METAAS_TEST_GAS
        options.backend = METAAS_GAS; options.format = METAAS_OBJECT;
        metaas::Assembler native(options);
        native.assembler_argument("--fatal-warnings");
        auto object = p.compile(native);
        check(object.bytes().size() > 4 && object.bytes()[0] == 0x7f && object.bytes()[1] == 'E');
#endif
        auto result_moved = std::move(result); check(result_moved.text() == "c3\n" && result.bytes().empty());
        std::cout << checks << " MetaAS DSL checks passed\n";
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
