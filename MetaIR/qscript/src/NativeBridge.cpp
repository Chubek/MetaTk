// Native x86-64 entry and C-ABI call adapters, encoded by ASMtk from Arch.
// The separately built static runtime supplies only the existing built-ins.
#include "qscript/src/Runtime.h"
#include "qscript/include/QScript-Native.h"
#include <sstream>

namespace qscript::detail {
Result<std::string> linked_native_source(const Unit& unit, const Options& options) {
    auto verified = verify_ir(unit.program, options);
    if (!verified) return {std::nullopt, std::move(verified.diagnostics)};
    try {
        std::ostringstream out;
        const auto bound = [&] {
            if (out.tellp() < 0 || static_cast<std::uint64_t>(out.tellp()) > options.toolchain.assembler.max_source_bytes)
                throw std::length_error("native assembly size limit exceeded");
        };
        const auto bytes = [&](std::string_view label, std::string_view text) {
            out << label << ":\n";
            std::size_t i = 0;
            for (unsigned char c : text) {
                out << (i++ % 16 ? "," : ".byte ") << static_cast<unsigned>(c);
                if (i % 16 == 0) { out << '\n'; bound(); }
            }
            if (i % 16) out << '\n';
            out << ".byte 0\n"; bound();
        };
        out << "; QScript static parser runtime ABI: link MetaChain::QScriptNativeRuntime\n"
            << ".extern qs_native_dispatch\nqs_rt_builtin:\n"
            << "push rbx\npush r12\nmov rbx, rsp\nsub rsp, 272\nand rsp, -16\nmov r12, rdi\n";
        for (unsigned i = 0; i < 32; ++i)
            out << "mov rax, [r12-" << (i + 1) * 8 << "]\nmov [rsp+" << i * 8 << "], rax\n";
        out << "mov rdi, r13\nmov rdx, rsp\nlea rcx, [rsp+256]\ncall qs_native_dispatch\n"
            << "cmp eax, 0\njne qs_rt_native_error\nmov rax, [rsp+256]\nmov rsp, rbx\npop r12\npop rbx\nret\n"
            << "qs_rt_native_error:\nmov edi, eax\nmov rsp, rbx\npop r12\npop rbx\njmp .mc_exit\n";
        for (std::size_t i = 0; i < unit.strings.size(); ++i) bytes("qs_native_str_" + std::to_string(i), unit.strings[i]);
        for (std::size_t i = 0; i < unit.types.size(); ++i) bytes("qs_native_type_" + std::to_string(i), unit.types[i]);
        for (std::size_t i = 0; i < unit.grammars.size(); ++i) {
            bytes("qs_native_name_" + std::to_string(i), unit.grammars[i].name);
            bytes("qs_native_grammar_" + std::to_string(i), unit.grammars[i].source);
        }
        out << ".align 8\nqs_native_strings:\n";
        for (std::size_t i = 0; i < unit.strings.size(); ++i) {
            out << ".quad qs_native_str_" << i << ',' << unit.strings[i].size() << '\n'; bound();
        }
        out << ".quad 0,0\nqs_native_types:\n";
        for (std::size_t i = 0; i < unit.types.size(); ++i) { out << ".quad qs_native_type_" << i << '\n'; bound(); }
        out << ".quad 0\nqs_native_grammars:\n";
        for (std::size_t i = 0; i < unit.grammars.size(); ++i) {
            out << ".quad qs_native_name_" << i << ",qs_native_grammar_" << i << ',' << unit.grammars[i].source.size() << '\n'; bound();
        }
        out << ".quad 0,0,0\n.global qs_native_unit_data\nqs_native_unit_data:\n.quad " << QS_NATIVE_ABI << '\n'
            << ".quad qs_native_strings," << unit.strings.size() << ",qs_native_types," << unit.types.size()
            << ",qs_native_grammars," << unit.grammars.size() << '\n'
            << ".quad " << options.max_heap_objects << ',' << options.max_heap_bytes << '\n'
            << ".quad " << options.toolchain.il.max_source_bytes << ',' << options.toolchain.il.max_instructions << '\n';
        bound();
        metachain::NativeRuntime runtime;
        runtime.entry = ".text\n.global qs_native_entry\nqs_native_entry:\npush rbp\npush rbx\npush r12\npush r13\npush r14\npush r15\n";
        runtime.prologue = "mov r13, rdi\n";
        runtime.builtin = "qs_rt_builtin";
        runtime.exit = "mov rax, rdi\nmov rsp, rbp\npop r15\npop r14\npop r13\npop r12\npop rbx\npop rbp\nret\n";
        runtime.text = out.str();
        return metachain::Toolchain(options.toolchain).native_source(unit.program, runtime);
    } catch (const std::exception& error) {
        return {std::nullopt, {{metachain::Stage::Lower, error.what(), {}, 0, 0, 0}}};
    }
}
} // namespace qscript::detail
