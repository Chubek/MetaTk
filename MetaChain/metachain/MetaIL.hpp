#pragma once

// Portable, deterministic integer register IL, text/binary codecs, verifier,
// conservative optimizer, builder, and a concrete MetaVM instruction set.
#include "Common.hpp"
#include "MetaVM.hpp"
#include <bit>
#include <array>
#include <span>
#include <sstream>
#include <map>

namespace metail {
enum class Op : std::uint8_t {
    Nop, Const, Mov, Add, Sub, Mul, Div, Mod, And, Or, Xor, Shl, Shr,
    Eq, Lt, Jmp, Jz, Jnz, Push, Pop, Call, Ret, Print, Halt
};
inline constexpr std::array<std::string_view, 24> op_names{
    "nop", "const", "mov", "add", "sub", "mul", "div", "mod", "and", "or", "xor", "shl", "shr",
    "eq", "lt", "jmp", "jz", "jnz", "push", "pop", "call", "ret", "print", "halt"};
inline std::string_view name(Op op) {
    const auto n = static_cast<std::size_t>(op);
    return n < op_names.size() ? op_names[n] : std::string_view{"invalid"};
}
inline std::optional<Op> opcode(std::string_view s) {
    for (std::size_t i = 0; i < op_names.size(); ++i) if (s == op_names[i]) return static_cast<Op>(i);
    return std::nullopt;
}
struct Instruction {
    Op op = Op::Nop;
    std::uint8_t a = 0, b = 0, c = 0;
    std::int64_t immediate = 0;
    bool operator==(const Instruction&) const = default;
};
using Program = metavm::Program<Instruction>;
struct Diagnostic {
    std::string message;
    std::size_t instruction = 0;
    metavm::SourceLocation source{};
};
struct Result {
    Program program;
    std::vector<Diagnostic> diagnostics;
    explicit operator bool() const noexcept { return diagnostics.empty(); }
};
struct Limits {
    std::size_t max_instructions = 1'000'000;
    std::size_t max_source_bytes = 64u * 1024u * 1024u;
};
inline bool binary(Op op) { return op >= Op::Add && op <= Op::Lt; }
inline bool branch(Op op) { return op == Op::Jmp || op == Op::Jz || op == Op::Jnz || op == Op::Call; }
inline unsigned registers(Op op) {
    if (binary(op)) return 3;
    if (op == Op::Mov) return 2;
    if (op == Op::Const || op == Op::Jz || op == Op::Jnz || op == Op::Push || op == Op::Pop || op == Op::Print || op == Op::Halt) return 1;
    return 0;
}
inline std::vector<Diagnostic> verify(const Program& p, Limits limits = {}) {
    std::vector<Diagnostic> errors;
    auto fail = [&](std::string m, std::size_t pc) {
        errors.push_back({std::move(m), pc, p.source_at(pc) ? *p.source_at(pc) : metavm::SourceLocation{}});
    };
    if (p.code.empty()) fail("empty IL program", 0);
    if (p.size() > limits.max_instructions) { fail("instruction limit exceeded", 0); return errors; }
    if (!p.source_map.empty() && p.source_map.size() != p.size()) fail("source map size differs from code size", 0);
    for (std::size_t pc = 0; pc < p.size(); ++pc) {
        const auto& i = p.code[pc];
        if (static_cast<std::size_t>(i.op) >= op_names.size()) { fail("invalid opcode", pc); continue; }
        const auto n = registers(i.op);
        if ((n > 0 && i.a >= 32) || (n > 1 && i.b >= 32) || (n > 2 && i.c >= 32)) fail("register index must be in [0, 31]", pc);
        if ((n == 0 && i.a) || (n < 2 && i.b) || (n < 3 && i.c)) fail("nonzero unused register field", pc);
        if (branch(i.op)) {
            if (i.immediate < 0 || static_cast<std::uint64_t>(i.immediate) >= p.size()) fail("branch target outside program", pc);
        } else if (i.op != Op::Const && i.immediate != 0) fail("nonzero unused immediate field", pc);
    }
    return errors;
}
inline std::optional<std::uint8_t> parse_register(std::string_view s) {
    if (s.size() < 2 || s[0] != 'r') return std::nullopt;
    unsigned n = 0;
    auto [p, ec] = std::from_chars(s.data() + 1, s.data() + s.size(), n);
    if (ec != std::errc{} || p != s.data() + s.size() || n >= 32) return std::nullopt;
    return static_cast<std::uint8_t>(n);
}
inline Result parse(std::string_view source, std::string file = {}, Limits limits = {}) {
    Result r; r.program.name = file;
    if (source.size() > limits.max_source_bytes) { r.diagnostics.push_back({"source size limit exceeded", 0, {file, 1, 1}}); return r; }
    std::map<std::string, std::size_t> labels;
    struct Fixup { std::size_t pc; std::string label; };
    std::vector<Fixup> fixups;
    std::size_t pos = 0; std::uint32_t line_no = 1;
    while (pos <= source.size()) {
        auto end = source.find('\n', pos); if (end == source.npos) end = source.size();
        auto line = metachain::text::trim(metachain::text::uncomment(source.substr(pos, end - pos)));
        metavm::SourceLocation loc{file, line_no, source.empty() ? 1u : static_cast<std::uint32_t>(line.data() - source.data() - pos + 1)};
        auto fail = [&](std::string m) { r.diagnostics.push_back({std::move(m), r.program.size(), loc}); };
        while (!line.empty()) {
            auto colon = line.find(':');
            if (colon == line.npos) break;
            auto label = metachain::text::trim(line.substr(0, colon));
            if (!metachain::text::identifier(label)) break;
            if (!labels.emplace(std::string(label), r.program.size()).second) fail("duplicate label: " + std::string(label));
            line = metachain::text::trim(line.substr(colon + 1));
        }
        if (!line.empty()) {
            const auto split = line.find_first_of(" \t");
            auto op = opcode(metachain::text::lower(line.substr(0, split)));
            auto args = metachain::text::operands(split == line.npos ? std::string_view{} : line.substr(split));
            if (!op) fail("unknown IL opcode");
            else {
                Instruction i; i.op = *op;
                const unsigned nr = registers(*op);
                const unsigned count = nr + ((*op == Op::Const || branch(*op)) ? 1 : 0);
                // mov/binary use only registers; branches carry one target.
                if (args.size() != count) fail("wrong operand count for " + std::string(name(*op)));
                else {
                    bool valid = true;
                    std::uint8_t* fields[] = {&i.a, &i.b, &i.c};
                    for (unsigned n = 0; n < nr; ++n) {
                        auto reg = parse_register(args[n]);
                        if (!reg) { fail("expected register r0..r31"); valid = false; }
                        else *fields[n] = *reg;
                    }
                    if (*op == Op::Const) {
                        auto v = metachain::text::signed_integer(args.back());
                        if (!v) { fail("expected signed 64-bit constant"); valid = false; } else i.immediate = *v;
                    } else if (branch(*op)) {
                        auto v = metachain::text::signed_integer(args.back());
                        if (v) i.immediate = *v;
                        else if (metachain::text::identifier(args.back())) fixups.push_back({r.program.size(), std::string(args.back())});
                        else { fail("invalid branch target"); valid = false; }
                    }
                    if (valid) {
                        if (r.program.size() >= limits.max_instructions) { fail("instruction limit exceeded"); return r; }
                        r.program.code.push_back(i); r.program.source_map.push_back(loc);
                    }
                }
            }
        }
        if (end == source.size()) break;
        pos = end + 1; ++line_no;
    }
    if (!r.diagnostics.empty()) { r.program.code.clear(); r.program.source_map.clear(); return r; }
    for (const auto& f : fixups) {
        auto it = labels.find(f.label);
        if (it == labels.end()) r.diagnostics.push_back({"undefined label: " + f.label, f.pc, r.program.source_map[f.pc]});
        else r.program.code[f.pc].immediate = static_cast<std::int64_t>(it->second);
    }
    auto errors = verify(r.program, limits);
    r.diagnostics.insert(r.diagnostics.end(), errors.begin(), errors.end());
    return r;
}
inline std::string emit(const Program& p) {
    std::ostringstream out;
    for (const auto& i : p.code) {
        out << name(i.op); unsigned nr = registers(i.op);
        const unsigned fields[] = {i.a, i.b, i.c};
        for (unsigned n = 0; n < nr; ++n) out << (n ? ", r" : " r") << fields[n];
        if (i.op == Op::Const || branch(i.op)) out << (nr ? ", " : " ") << i.immediate;
        out << '\n';
    }
    return out.str();
}
// Wire format: MCIL, version=1, reserved=0 (three bytes), count:u32le,
// then count records of {opcode,a,b,c,immediate:i64le}. No native struct layout.
inline std::vector<std::uint8_t> encode(const Program& p) {
    if (!verify(p).empty()) return {};
    std::vector<std::uint8_t> out{'M','C','I','L',1,0,0,0};
    auto put = [&](std::uint64_t v, unsigned n) { for (unsigned j = 0; j < n; ++j) out.push_back(static_cast<std::uint8_t>(v >> (8*j))); };
    put(p.size(), 4);
    for (const auto& i : p.code) {
        out.push_back(static_cast<std::uint8_t>(i.op)); out.push_back(i.a); out.push_back(i.b); out.push_back(i.c);
        put(std::bit_cast<std::uint64_t>(i.immediate), 8);
    }
    return out;
}
inline Result decode(std::span<const std::uint8_t> bytes, Limits limits = {}) {
    Result r;
    auto fail = [&](std::string m) { r.diagnostics.push_back({std::move(m), 0, {}}); };
    if (bytes.size() < 12 || bytes[0] != 'M' || bytes[1] != 'C' || bytes[2] != 'I' || bytes[3] != 'L' ||
            bytes[4] != 1 || bytes[5] || bytes[6] || bytes[7]) { fail("invalid MCIL header or version"); return r; }
    auto get = [&](std::size_t p, unsigned n) { std::uint64_t v = 0; for (unsigned j = 0; j < n; ++j) v |= std::uint64_t(bytes[p+j]) << (j*8); return v; };
    const auto count = get(8, 4);
    if (count > limits.max_instructions || count > (bytes.size() - 12)/12 || bytes.size() - 12 != count*12) {
        fail("invalid MCIL length or instruction limit exceeded"); return r;
    }
    r.program.code.reserve(static_cast<std::size_t>(count));
    for (std::size_t p = 12; p < bytes.size(); p += 12)
        r.program.code.push_back({static_cast<Op>(bytes[p]), bytes[p+1], bytes[p+2], bytes[p+3], std::bit_cast<std::int64_t>(get(p+4, 8))});
    r.diagnostics = verify(r.program, limits); return r;
}
inline std::optional<std::int64_t> evaluate(Op op, std::int64_t a, std::int64_t b) {
    const auto x = std::bit_cast<std::uint64_t>(a), y = std::bit_cast<std::uint64_t>(b);
    auto bits = [](std::uint64_t v) { return std::bit_cast<std::int64_t>(v); };
    switch (op) {
    case Op::Add: return bits(x+y); case Op::Sub: return bits(x-y); case Op::Mul: return bits(x*y);
    case Op::Div: case Op::Mod:
        if (b == 0 || (a == INT64_MIN && b == -1)) return std::nullopt;
        return op == Op::Div ? a/b : a%b;
    case Op::And: return bits(x&y); case Op::Or: return bits(x|y); case Op::Xor: return bits(x^y);
    case Op::Shl: case Op::Shr:
        if (b < 0 || b > 63) return std::nullopt;
        return bits(op == Op::Shl ? x << y : x >> y);
    case Op::Eq: return a == b ? 1 : 0; case Op::Lt: return a < b ? 1 : 0;
    default: return std::nullopt;
    }
}
struct ISA {
    static constexpr std::size_t register_count = 32;
    template<class VM> static bool execute(VM& vm, const Instruction& i) {
        auto get = [&](unsigned n) { const auto& v = vm.reg(n); return v.kind == metavm::Value::Kind::Empty ? std::int64_t{0} : v.as_integer(); };
        auto set = [&](unsigned n, std::int64_t v) { vm.reg(n) = metavm::Value::integer(v); };
        if (binary(i.op)) {
            const auto value = evaluate(i.op, get(i.b), get(i.c));
            if (!value) { vm.trap("invalid division, remainder, or shift"); return false; }
            set(i.a, *value); return true;
        }
        switch (i.op) {
        case Op::Nop: break;
        case Op::Const: set(i.a, i.immediate); break;
        case Op::Mov: set(i.a, get(i.b)); break;
        case Op::Jmp: vm.set_pc(static_cast<std::size_t>(i.immediate)); break;
        case Op::Jz: case Op::Jnz:
            if ((get(i.a) == 0) == (i.op == Op::Jz)) vm.set_pc(static_cast<std::size_t>(i.immediate));
            break;
        case Op::Push: case Op::Call:
            if (vm.stack().size() >= vm.options().max_stack_values) { vm.trap("stack limit exceeded"); return false; }
            vm.stack().push_back(metavm::Value::integer(i.op == Op::Push ? get(i.a) : static_cast<std::int64_t>(vm.pc()+1)));
            if (i.op == Op::Call) vm.set_pc(static_cast<std::size_t>(i.immediate));
            break;
        case Op::Pop: case Op::Ret: {
            if (vm.stack().empty()) { vm.trap("stack underflow"); return false; }
            const auto v = vm.stack().back().as_integer(); vm.stack().pop_back();
            if (i.op == Op::Pop) set(i.a, v);
            else {
                if (v < 0 || static_cast<std::uint64_t>(v) > vm.program().size()) { vm.trap("invalid return address"); return false; }
                vm.set_pc(static_cast<std::size_t>(v));
            }
            break;
        }
        case Op::Print: vm.output(metavm::Value::integer(get(i.a))); break;
        case Op::Halt: return false;
        default: vm.trap("invalid IL opcode"); return false;
        }
        return true;
    }
};
using VM = metavm::VM<Instruction, ISA>;
struct Execution {
    metavm::Status status = metavm::Status::Ready;
    std::int64_t value = 0;
    std::vector<std::int64_t> output;
    std::size_t steps = 0;
    std::vector<Diagnostic> diagnostics;
    explicit operator bool() const noexcept { return status == metavm::Status::Halted && diagnostics.empty(); }
};
// Caller-supplied programs are verified against `limits`; pass coordinated
// ceilings when the facade configuration must bound every stage.
inline Execution run(const Program& p, const metavm::VMOptions& options = {}, const Limits& limits = {}) {
    Execution r; r.diagnostics = verify(p, limits);
    if (!r.diagnostics.empty()) { r.status = metavm::Status::Trapped; return r; }
    VM vm(options); vm.load(p);
    vm.set_output_sink([&](const metavm::Value& value) { r.output.push_back(value.as_integer()); });
    // Halt is observed before dispatch so the operand is still live. The
    // loop stops on the first terminal status, so the value is captured once.
    for (;;) {
        if (vm.status() != metavm::Status::Running && vm.status() != metavm::Status::Ready) break;
        if (vm.pc() < p.size()) {
            const auto& i = p.code[vm.pc()];
            if (i.op == Op::Halt) {
                const auto& v = vm.reg(i.a < 32 ? i.a : 0);
                r.value = v.kind == metavm::Value::Kind::Empty ? 0 : v.as_integer();
            }
        }
        if (!vm.step() && vm.status() != metavm::Status::Running) break;
    }
    r.status = vm.status(); r.steps = vm.steps();
    if (vm.diagnostic()) r.diagnostics.push_back({vm.diagnostic()->message, vm.diagnostic()->pc, vm.diagnostic()->source});
    if (r.status == metavm::Status::StepLimit) r.diagnostics.push_back({"execution step limit exceeded", vm.pc(), {}});
    return r;
}
inline Result optimize(Program p) {
    Result r; r.diagnostics = verify(p); r.program = std::move(p);
    if (!r.diagnostics.empty()) return r;
    std::vector<bool> leaders(r.program.size()); leaders[0] = true;
    for (std::size_t pc = 0; pc < r.program.size(); ++pc) {
        const auto& i = r.program.code[pc];
        if (branch(i.op)) leaders[static_cast<std::size_t>(i.immediate)] = true;
        if ((branch(i.op) || i.op == Op::Ret || i.op == Op::Halt) && pc+1 < leaders.size()) leaders[pc+1] = true;
    }
    std::array<std::optional<std::int64_t>, 32> constants{};
    for (std::size_t pc = 0; pc < r.program.size(); ++pc) {
        if (leaders[pc]) constants.fill(std::nullopt);
        auto& i = r.program.code[pc];
        std::optional<std::int64_t> v;
        if (i.op == Op::Const) v = i.immediate;
        else if (i.op == Op::Mov) v = constants[i.b];
        else if (binary(i.op) && constants[i.b] && constants[i.c]) v = evaluate(i.op, *constants[i.b], *constants[i.c]);
        if (i.op == Op::Const || i.op == Op::Mov || binary(i.op) || i.op == Op::Pop) {
            constants[i.a] = v;
            if (v) i = {Op::Const, i.a, 0, 0, *v};
        }
        if (branch(i.op) || i.op == Op::Ret) constants.fill(std::nullopt);
    }
    return r;
}
class Builder : public dsl::DSL<Builder, dsl::Pipeline> {
    Program p_;
    std::map<std::string, std::size_t> labels_;
    std::vector<std::pair<std::size_t, std::string>> fixups_;
    std::vector<Diagnostic> errors_;
public:
    Builder& name(std::string n) { p_.name = std::move(n); return *this; }
    Builder& emit(Instruction i, metavm::SourceLocation s = {}) { p_.code.push_back(i); p_.source_map.push_back(std::move(s)); return *this; }
    Builder& label(std::string n) {
        if (!metachain::text::identifier(n) || !labels_.emplace(n, p_.size()).second) errors_.push_back({"invalid or duplicate label: " + n, p_.size(), {}});
        return *this;
    }
    Builder& constant(std::uint8_t a, std::int64_t v) { return emit({Op::Const, a, 0, 0, v}); }
    Builder& binary(Op op, std::uint8_t a, std::uint8_t b, std::uint8_t c) {
        if (!metail::binary(op)) errors_.push_back({"binary requires an arithmetic/comparison opcode", p_.size(), {}});
        return emit({op, a, b, c, 0});
    }
    Builder& jump(Op op, std::string target, std::uint8_t test = 0) {
        if (!branch(op)) errors_.push_back({"jump requires a branch opcode", p_.size(), {}});
        fixups_.push_back({p_.size(), std::move(target)}); return emit({op, test, 0, 0, 0});
    }
    Builder& halt(std::uint8_t a = 0) { return emit({Op::Halt, a, 0, 0, 0}); }
    Result finish() const {
        Result r{p_, errors_};
        for (const auto& [pc, n] : fixups_) {
            auto it = labels_.find(n);
            if (it == labels_.end()) r.diagnostics.push_back({"undefined label: " + n, pc, {}});
            else r.program.code[pc].immediate = static_cast<std::int64_t>(it->second);
        }
        auto errors = verify(r.program); r.diagnostics.insert(r.diagnostics.end(), errors.begin(), errors.end()); return r;
    }
};
} // namespace metail
