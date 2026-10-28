#include <juniper/MetaArch.hpp>
#include <fstream>
#include <iostream>
#include <stdexcept>

template<class T> T take(juniper::Result<T> result) {
    if (!result) throw std::runtime_error(juniper::format(*result.error()));
    return std::move(*result.get());
}
static void write(const std::filesystem::path& file, const std::string& text) {
    std::ofstream out(file); out << text; out.close();
    if (!out) throw std::runtime_error("cannot write " + file.string());
}
int main(int argc, char** argv) {
    try {
        if (argc != 4) return 2;
        const std::filesystem::path work = argv[1];
        juniper::Compiler compiler;
        auto module = take(compiler.parse(
            "extern fn magnitude(i64) -> i64 from \"c\" symbol \"labs\";\n"
            "extern fn twice(i64) -> i64 from " + mair::quote(argv[2]) + " symbol \"custom_twice\";\n"
            "fn foreign_test(x) { return twice(magnitude(x)); }\n"));
        module = take(compiler.optimize(module));
        module = take(compiler.parse(take(compiler.emit(module))));
        auto machine = take(juniper::schedule(take(juniper::select(module))));
        const auto& instructions = machine.functions.at(0).instructions;
        if (instructions.size() != 3 || instructions[0].opcode != "CALL" || instructions[1].opcode != "CALL" ||
            instructions[2].opcode != "RET") throw std::runtime_error("unexpected selected function");
        auto target = mair::target("amd64", argv[3]);
        for (std::size_t i = 0; i < 2; ++i) {
            auto image = take(juniper::encode_external_call(machine, instructions[i], {"call", "sym", -4, {}}, target));
            if (image.imports.size() != 2 || image.relocations.at(0).symbol != machine.externals[i].symbol ||
                image.relocations[0].value != -4) throw std::runtime_error("call relocation/import was lost");
        }
        mair::Program program;
        for (const auto& f : machine.externals) program.extern_function(f.name, f.library, f.symbol);
        // Explicit SysV AMD64 ABI setup for this unary signed-long fixture only.
        // This deliberately is not a hidden/general Juniper ABI lowerer.
        program.section(".text").global("foreign_test").label("foreign_test")
            .ins("subq", "$8", "%rsp")
            .call_external(machine.externals[*instructions[0].external].name)
            .ins("movq", "%rax", "%rdi")
            .call_external(machine.externals[*instructions[1].external].name)
            .ins("addq", "$8", "%rsp").ins("ret");
        write(work / "foreign.s", mair::render(program, target));
        write(work / "foreign.link.json", take(compiler.linkage(module)));
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
