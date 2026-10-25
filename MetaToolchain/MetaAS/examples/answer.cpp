#include <MetaAS/MetaAS-DSL.hpp>
#include <iostream>

int main(int argc, char **argv) {
    try {
        auto options = metaas::options();
        options.target = "amd64";
        options.backend = METAAS_BUILTIN;
        options.format = METAAS_HEX;
        if (argc > 1) options.database = argv[1];
        metaas::Assembler assembler(options);
        using namespace metaas::dsl;
        auto program = Program{} | section(".text") | global("answer") | label("answer")
            | encode("movimm", {arg<"dst">(reg("rax")), arg<"imm">(imm(42))})
            | encode("ret");
        auto result = program | compile(assembler, "answer.cpp");
        std::cout << result.text();
        return result.text() == "48b82a00000000000000c3\n" ? 0 : 1;
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
