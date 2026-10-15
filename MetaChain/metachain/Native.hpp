#pragma once
#include "MetaIL.hpp"

namespace metachain {
struct NativeSource {
    std::string assembly;
    std::vector<metail::Diagnostic> diagnostics;
    explicit operator bool() const noexcept { return diagnostics.empty(); }
};
// Complete MetaIL lowering to the Linux x86-64 syscall ABI. IL registers live
// in a fixed stack frame; r14/r15 track execution and operand-stack budgets.
inline NativeSource lower_linux_x86_64(const metail::Program& p, metavm::VMOptions options = {},
        std::size_t max_source_bytes = 64u * 1024u * 1024u) {
    using metail::Op;
    NativeSource r; r.diagnostics = metail::verify(p);
    if (!r.diagnostics.empty()) return r;
    std::ostringstream out;
    auto slot = [](unsigned reg) { return "[rbp-" + std::to_string((reg+1)*8) + "]"; };
    auto label = [](std::int64_t pc) { return ".mc_pc_" + std::to_string(pc); };
    auto load = [&](std::string_view machine, unsigned reg) { out << "mov " << machine << ", " << slot(reg) << '\n'; };
    auto store = [&](unsigned reg, std::string_view machine) { out << "mov " << slot(reg) << ", " << machine << '\n'; };
    auto push = [&] {
        out << "mov rcx, " << options.max_stack_values << "\ncmp r15, rcx\njae .mc_stack_error\npush rax\ninc r15\n";
    };
    auto pop = [&] { out << "cmp r15, 0\nje .mc_stack_error\npop rax\ndec r15\n"; };
    out << ".text\n.global _start\n_start:\nmov rbp, rsp\nsub rsp, 256\nxor rax, rax\nxor r14, r14\nxor r15, r15\n";
    for (unsigned n = 0; n < 32; ++n) store(n,"rax");
    for (std::size_t pc = 0; pc < p.size(); ++pc) {
        const auto& i = p.code[pc];
        out << label(static_cast<std::int64_t>(pc)) << ":\nmov rax, " << options.max_steps << "\ncmp r14, rax\njae .mc_step_error\ninc r14\n";
        if (metail::binary(i.op)) {
            load("rax",i.b); load("rcx",i.c);
            switch (i.op) {
            case Op::Add: case Op::Sub: case Op::And: case Op::Or: case Op::Xor:
                out << metail::name(i.op) << " rax, rcx\n"; break;
            case Op::Mul: out << "imul rax, rcx\n"; break;
            case Op::Div: case Op::Mod:
                out << "cmp rcx, 0\nje .mc_arithmetic_error\ncmp rcx, -1\njne .mc_div_ok_" << pc
                    << "\nmov rdx, -9223372036854775808\ncmp rax, rdx\nje .mc_arithmetic_error\n.mc_div_ok_" << pc
                    << ":\ncqo\nidiv rcx\n";
                if (i.op == Op::Mod) out << "mov rax, rdx\n";
                break;
            case Op::Shl: case Op::Shr:
                out << "cmp rcx, 0\njl .mc_arithmetic_error\ncmp rcx, 63\njg .mc_arithmetic_error\n"
                    << metail::name(i.op) << " rax, cl\n"; break;
            case Op::Eq: case Op::Lt:
                out << "cmp rax, rcx\nmov rax, 0\n" << (i.op == Op::Eq ? "jne" : "jge") << " .mc_compare_done_" << pc
                    << "\nmov rax, 1\n.mc_compare_done_" << pc << ":\n"; break;
            default: break;
            }
            store(i.a,"rax");
        } else switch (i.op) {
        case Op::Nop: out << "nop\n"; break;
        case Op::Const: out << "mov rax, " << i.immediate << '\n'; store(i.a,"rax"); break;
        case Op::Mov: load("rax",i.b); store(i.a,"rax"); break;
        case Op::Jmp: out << "jmp " << label(i.immediate) << '\n'; break;
        case Op::Jz: case Op::Jnz:
            load("rax",i.a); out << "cmp rax, 0\n" << (i.op == Op::Jz ? "je " : "jne ") << label(i.immediate) << '\n'; break;
        case Op::Push: load("rax",i.a); push(); break;
        case Op::Pop: pop(); store(i.a,"rax"); break;
        case Op::Call: out << "mov rax, " << pc+1 << '\n'; push(); out << "jmp " << label(i.immediate) << '\n'; break;
        case Op::Ret:
            pop(); out << "cmp rax, 0\njl .mc_stack_error\nmov rcx, " << p.size()
                << "\ncmp rax, rcx\nja .mc_stack_error\nlea rdx, [rel .mc_dispatch]\nshl rax, 3\nadd rax, rdx\nmov rcx, [rax]\njmp rcx\n"; break;
        case Op::Halt:
            load("rdi",i.a); out << "mov rax, 60\nsyscall\nud2\n"; break;
        case Op::Print:
            load("rax",i.a);
            out << "sub rsp, 64\nlea rsi, [rsp+63]\nmov rdx, 10\nmov [rsi], dl\nmov r9, 1\nmov r10, 0\ncmp rax, 0\njge .mc_digits_" << pc
                << "\nneg rax\nmov r10, 1\n.mc_digits_" << pc << ":\nxor rdx, rdx\nmov rcx, 10\ndiv rcx\nadd rdx, 48\ndec rsi\nmov [rsi], dl\ninc r9\ncmp rax, 0\njne .mc_digits_" << pc
                << "\ncmp r10, 0\nje .mc_write_begin_" << pc << "\ndec rsi\nmov rdx, 45\nmov [rsi], dl\ninc r9\n.mc_write_begin_" << pc
                << ":\nmov rdx, r9\n.mc_write_" << pc << ":\nmov rax, 1\nmov rdi, 1\nsyscall\ncmp rax, -4\nje .mc_write_" << pc
                << "\ncmp rax, 0\njle .mc_io_error\nadd rsi, rax\nsub rdx, rax\ncmp rdx, 0\njne .mc_write_" << pc << "\nadd rsp, 64\n";
            break;
        default: break;
        }
        if (out.tellp() < 0 || static_cast<std::uint64_t>(out.tellp()) > max_source_bytes) {
            r.diagnostics.push_back({"native assembly size limit exceeded",pc,{}}); return r;
        }
    }
    out << label(static_cast<std::int64_t>(p.size())) << ":\nmov rdi, 0\njmp .mc_exit\n"
        << ".mc_step_error:\nmov rdi, 124\njmp .mc_exit\n.mc_stack_error:\nmov rdi, 125\njmp .mc_exit\n"
        << ".mc_arithmetic_error:\nmov rdi, 126\njmp .mc_exit\n.mc_io_error:\nmov rdi, 127\n.mc_exit:\nmov rax, 60\nsyscall\nud2\n.align 8\n.mc_dispatch:\n";
    for (std::size_t pc = 0; pc <= p.size(); ++pc) out << ".quad " << label(static_cast<std::int64_t>(pc)) << '\n';
    if (out.tellp() < 0 || static_cast<std::uint64_t>(out.tellp()) > max_source_bytes) r.diagnostics.push_back({"native assembly size limit exceeded",0,{}});
    else r.assembly = out.str();
    return r;
}
} // namespace metachain
