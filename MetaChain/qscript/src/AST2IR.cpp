// QScript lowering to MetaIL.
//
// Register convention (per function):
//   r0..r15   variables and parameters (block-scoped allocation)
//   r16..r29  expression temporaries
//   r30       return value of the most recent call
//   r31       scratch (return address in prologues, immediate operands)
//
// Calls: the caller pushes its live variables and in-use temporaries, then
// each argument, then `call`s. The callee pops its return address into r31,
// pops its arguments into r0..r(n-1), pushes the return address back, and
// returns with the value in r30. The caller restores its registers and
// copies r30 into the target temporary. All of this uses only MetaIL's
// existing push/pop/call/ret, so the interpreter, JIT and the x86-64
// lowering run QScript functions unchanged.
//
// Layout: the program's top-level statements come first and end in
// `halt`; reachable functions follow. A failed `assert` executes
// `const r31, 0; div r31, r31, r31`, which traps and is reported as an
// assertion failure by qscript::run.
#include "qscript/QScript.h"
#include "qscript/src/Analysis.h"
#include <map>
#include <set>
#include <stdexcept>

namespace qscript {
namespace {
using metail::Op;
using detail::Analysis;
using detail::arity;

class Lowerer {
    const Options& o_;
    const Analysis& a_;
    Unit unit_;
    metail::Program& p_;
    metavm::SourceLocation where_;
    struct Fixup { std::size_t pc; std::string function; };
    std::vector<Fixup> calls_;
    // Current function state.
    std::string fn_unit_;
    bool in_function_ = false;
    std::vector<std::map<std::string, unsigned>> scopes_;
    std::set<unsigned> free_;
    struct Loop { std::vector<std::size_t> breaks, continues; };
    std::vector<Loop> loops_;

    static std::uint8_t reg(unsigned n) { return static_cast<std::uint8_t>(n); }
    std::size_t emit(metail::Instruction i) {
        if (p_.size() >= o_.toolchain.il.max_instructions) throw std::invalid_argument("instruction limit exceeded");
        p_.code.push_back(i); p_.source_map.push_back(where_);
        return p_.size() - 1;
    }
    std::size_t here() const { return p_.size(); }
    void patch(std::size_t pc, std::size_t target) { p_.code[pc].immediate = static_cast<std::int64_t>(target); }
    void locate(const dsl::ASTNode& n) {
        auto l = detail::location(n);
        if (l.line) where_ = {l.file, static_cast<std::uint32_t>(l.line), static_cast<std::uint32_t>(l.column)};
    }
    std::optional<unsigned> variable(const std::string& name) const {
        for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it)
            if (auto f = it->find(name); f != it->end()) return f->second;
        return std::nullopt;
    }
    std::int64_t constant(const std::string& name) const {
        std::string why;
        auto q = a_.constant(fn_unit_, name, why);
        if (q.empty()) throw std::invalid_argument(why.empty() ? "undefined variable: " + name : why);
        auto it = a_.constants.find(q);
        if (it == a_.constants.end() || !it->second.value) throw std::invalid_argument("invalid constant: " + name);
        return *it->second.value;
    }
    std::set<unsigned> live_variables() const {
        std::set<unsigned> r;
        for (const auto& s : scopes_) for (const auto& [n, v] : s) { (void)n; r.insert(v); }
        return r;
    }
    void enter() { scopes_.emplace_back(); }
    void leave() { for (auto [n, r] : scopes_.back()) { (void)n; free_.insert(r); } scopes_.pop_back(); }
    unsigned allocate(const std::string& name) {
        if (free_.empty()) throw std::invalid_argument("QScript supports at most 16 simultaneously live variables");
        unsigned r = *free_.begin(); free_.erase(free_.begin());
        scopes_.back()[name] = r;
        return r;
    }
    void check_temporary(unsigned t) const {
        if (t > detail::last_temporary) throw std::invalid_argument("expression requires more than 14 temporary registers; split it with let");
    }
    void normalize(unsigned t) {  // t = (t != 0)
        emit({Op::Const, reg(detail::scratch_register), 0, 0, 0});
        emit({Op::Eq, reg(t), reg(t), reg(detail::scratch_register), 0});
        emit({Op::Eq, reg(t), reg(t), reg(detail::scratch_register), 0});
    }
    void call(const std::string& qualified, const std::vector<const dsl::ASTNode*>& args, unsigned t) {
        check_temporary(t);
        std::vector<unsigned> saved;
        for (auto v : live_variables()) saved.push_back(v);
        for (unsigned r = detail::first_temporary; r < t; ++r) saved.push_back(r);
        for (auto r : saved) emit({Op::Push, reg(r), 0, 0, 0});
        for (const auto* arg : args) { expr(*arg, t); emit({Op::Push, reg(t), 0, 0, 0}); }
        calls_.push_back({emit({Op::Call, 0, 0, 0, 0}), qualified});
        for (auto it = saved.rbegin(); it != saved.rend(); ++it) emit({Op::Pop, reg(*it), 0, 0, 0});
        emit({Op::Mov, reg(t), reg(detail::return_register), 0, 0});
    }
    // Operand register for the right side of a binary operator.
    unsigned operand(const dsl::ASTNode& n, unsigned t) {
        if (n.is_leaf() && n.tag() == "integer") {
            emit({Op::Const, reg(detail::scratch_register), 0, 0, *metachain::text::signed_integer(n.value())});
            return detail::scratch_register;
        }
        if (n.is_leaf() && n.tag() == "name") {
            if (auto v = variable(n.value())) return *v;
            emit({Op::Const, reg(detail::scratch_register), 0, 0, constant(n.value())});
            return detail::scratch_register;
        }
        check_temporary(t + 1);
        expr(n, t + 1);
        return t + 1;
    }
    void expr(const dsl::ASTNode& n, unsigned t) {
        check_temporary(t);
        const auto tag = n.tag();
        if (n.is_leaf() && tag == "integer") { emit({Op::Const, reg(t), 0, 0, *metachain::text::signed_integer(n.value())}); return; }
        if (n.is_leaf() && tag == "name") {
            if (auto v = variable(n.value())) emit({Op::Mov, reg(t), reg(*v), 0, 0});
            else emit({Op::Const, reg(t), 0, 0, constant(n.value())});
            return;
        }
        if (tag == "call") {
            auto saved = where_;
            locate(n);
            std::string why;
            auto q = a_.function(fn_unit_, n.child(0).value(), why);
            if (q.empty()) throw std::invalid_argument(why);
            std::vector<const dsl::ASTNode*> args;
            for (std::size_t i = 1; i < arity(n); ++i) args.push_back(&n.child(i));
            call(q, args, t);
            where_ = saved;
            return;
        }
        if (tag == "**") { call(std::string(detail::core_unit) + "::pow", {&n.child(0), &n.child(1)}, t); return; }
        if (tag == "neg" || tag == "not" || tag == "invert") {
            expr(n.child(0), t);
            emit({Op::Const, reg(detail::scratch_register), 0, 0, tag == "invert" ? -1 : 0});
            if (tag == "neg") emit({Op::Sub, reg(t), reg(detail::scratch_register), reg(t), 0});
            else emit({tag == "not" ? Op::Eq : Op::Xor, reg(t), reg(t), reg(detail::scratch_register), 0});
            return;
        }
        if (tag == "&&" || tag == "||") {
            expr(n.child(0), t);
            if (tag == "&&") {
                auto skip = emit({Op::Jz, reg(t), 0, 0, 0});
                expr(n.child(1), t); normalize(t);
                patch(skip, here());
            } else {
                auto rhs = emit({Op::Jz, reg(t), 0, 0, 0});
                emit({Op::Const, reg(t), 0, 0, 1});
                auto end = emit({Op::Jmp, 0, 0, 0, 0});
                patch(rhs, here());
                expr(n.child(1), t); normalize(t);
                patch(end, here());
            }
            return;
        }
        if (tag == "?:") {
            expr(n.child(0), t);
            auto otherwise = emit({Op::Jz, reg(t), 0, 0, 0});
            expr(n.child(1), t);
            auto end = emit({Op::Jmp, 0, 0, 0, 0});
            patch(otherwise, here());
            expr(n.child(2), t);
            patch(end, here());
            return;
        }
        static const std::map<std::string_view, Op> ops{{"+", Op::Add}, {"-", Op::Sub}, {"*", Op::Mul}, {"/", Op::Div}, {"%", Op::Mod},
            {"&", Op::And}, {"|", Op::Or}, {"^", Op::Xor}, {"<<", Op::Shl}, {">>", Op::Shr}, {"==", Op::Eq}, {"!=", Op::Eq},
            {"<", Op::Lt}, {">", Op::Lt}, {"<=", Op::Lt}, {">=", Op::Lt}};
        auto op = ops.find(tag);
        if (op == ops.end() || arity(n) != 2) throw std::invalid_argument("invalid expression AST");
        expr(n.child(0), t);
        const unsigned r = operand(n.child(1), t);
        const bool reverse = tag == ">" || tag == "<=";
        emit({op->second, reg(t), reg(reverse ? r : t), reg(reverse ? t : r), 0});
        if (tag == "!=" || tag == "<=" || tag == ">=") {
            emit({Op::Const, reg(detail::scratch_register), 0, 0, 0});
            emit({Op::Eq, reg(t), reg(t), reg(detail::scratch_register), 0});
        }
    }
    void scoped(const dsl::ASTNode& n) { enter(); stmt(n); leave(); }
    void stmt(const dsl::ASTNode& n) {
        locate(n);
        const auto tag = n.tag();
        const unsigned t = detail::first_temporary;
        if (tag == "program" || tag == "block") {
            enter();
            for (std::size_t i = 0; i < arity(n); ++i) stmt(n.child(i));
            leave();
        } else if (tag == "let" || tag == "assign") {
            expr(n.child(1), t);
            const unsigned r = tag == "let" ? allocate(n.child(0).value()) : *variable(n.child(0).value());
            emit({Op::Mov, reg(r), reg(t), 0, 0});
        } else if (tag == "expr") {
            expr(n.child(0), t);
        } else if (tag == "print") {
            for (std::size_t i = 0; i < arity(n); ++i) { expr(n.child(i), t); emit({Op::Print, reg(t), 0, 0, 0}); }
        } else if (tag == "return" && in_function_) {
            expr(n.child(0), t);
            emit({Op::Mov, reg(detail::return_register), reg(t), 0, 0});
            emit({Op::Ret, 0, 0, 0, 0});
        } else if (tag == "return" || tag == "exit") {
            expr(n.child(0), t);
            emit({Op::Halt, reg(t), 0, 0, 0});
        } else if (tag == "assert") {
            expr(n.child(0), t);
            auto ok = emit({Op::Jnz, reg(t), 0, 0, 0});
            emit({Op::Const, reg(detail::scratch_register), 0, 0, 0});
            emit({Op::Div, reg(detail::scratch_register), reg(detail::scratch_register), reg(detail::scratch_register), 0});
            patch(ok, here());
        } else if (tag == "if") {
            expr(n.child(0), t);
            auto branch = emit({Op::Jz, reg(t), 0, 0, 0});
            scoped(n.child(1));
            if (arity(n) == 3) {
                auto end = emit({Op::Jmp, 0, 0, 0, 0});
                patch(branch, here());
                scoped(n.child(2));
                patch(end, here());
            } else patch(branch, here());
        } else if (tag == "while") {
            const auto start = here();
            expr(n.child(0), t);
            auto exit = emit({Op::Jz, reg(t), 0, 0, 0});
            loops_.emplace_back();
            scoped(n.child(1));
            locate(n);
            emit({Op::Jmp, 0, 0, 0, static_cast<std::int64_t>(start)});
            patch(exit, here());
            close_loop(here(), start);
        } else if (tag == "do") {
            const auto start = here();
            loops_.emplace_back();
            scoped(n.child(0));
            const auto test = here();
            locate(n);
            expr(n.child(1), t);
            emit({Op::Jnz, reg(t), 0, 0, static_cast<std::int64_t>(start)});
            close_loop(here(), test);
        } else if (tag == "for") {
            enter();
            stmt(n.child(0));
            locate(n);
            const auto start = here();
            expr(n.child(1), t);
            auto exit = emit({Op::Jz, reg(t), 0, 0, 0});
            loops_.emplace_back();
            scoped(n.child(3));
            const auto step = here();
            stmt(n.child(2));
            locate(n);
            emit({Op::Jmp, 0, 0, 0, static_cast<std::int64_t>(start)});
            patch(exit, here());
            close_loop(here(), step);
            leave();
        } else if (tag == "break" || tag == "continue") {
            auto jump = emit({Op::Jmp, 0, 0, 0, 0});
            (tag == "break" ? loops_.back().breaks : loops_.back().continues).push_back(jump);
        } else throw std::invalid_argument("invalid statement AST");
    }
    void close_loop(std::size_t break_target, std::size_t continue_target) {
        auto loop = std::move(loops_.back()); loops_.pop_back();
        for (auto pc : loop.breaks) patch(pc, break_target);
        for (auto pc : loop.continues) patch(pc, continue_target);
    }
    void reset(const std::string& unit, bool in_function) {
        fn_unit_ = unit; in_function_ = in_function;
        scopes_.clear(); loops_.clear(); free_.clear();
        for (unsigned n = 0; n < detail::max_variables; ++n) free_.insert(n);
    }
public:
    Lowerer(const Options& o, const Analysis& a) : o_(o), a_(a), p_(unit_.program) {}
    const metavm::SourceLocation& where() const { return where_; }
    Unit run() {
        // Program body.
        reset("", false);
        enter();
        for (const auto* s : a_.statements) stmt(*s);
        leave();
        where_ = {};
        emit({Op::Const, reg(detail::first_temporary), 0, 0, 0});
        emit({Op::Halt, reg(detail::first_temporary), 0, 0, 0});
        // Functions, in deterministic (name) order.
        std::map<std::string, std::size_t> entries;
        for (const auto& q : a_.used_functions) {
            const auto& f = a_.functions.at(q);
            reset(f.unit, true);
            locate(*f.decl);
            entries[q] = here();
            enter();
            emit({Op::Pop, reg(detail::scratch_register), 0, 0, 0});
            std::vector<unsigned> params;
            for (const auto& name : f.params) params.push_back(allocate(name));
            for (auto it = params.rbegin(); it != params.rend(); ++it) emit({Op::Pop, reg(*it), 0, 0, 0});
            emit({Op::Push, reg(detail::scratch_register), 0, 0, 0});
            stmt(*f.body);
            leave();
            emit({Op::Const, reg(detail::return_register), 0, 0, 0});
            emit({Op::Ret, 0, 0, 0, 0});
            const auto l = detail::location(*f.decl);
            unit_.functions.push_back({q, entries[q], f.params.size(), l.file, l.line});
        }
        for (const auto& fix : calls_) {
            auto it = entries.find(fix.function);
            if (it == entries.end()) throw std::invalid_argument("internal: call to unemitted function " + fix.function);
            patch(fix.pc, it->second);
        }
        unit_.libraries = a_.library_order;
        return std::move(unit_);
    }
};
} // namespace

Result<Unit> lower(const AST& ast, Options o) {
    Result<Unit> out;
    auto analysis = detail::analyze(ast, o);
    if (!analysis.diagnostics.empty()) { out.diagnostics = std::move(analysis.diagnostics); return out; }
    Lowerer lowerer(o, analysis);
    try {
        auto unit = lowerer.run();
        auto checked = verify_ir(std::move(unit.program), o);
        if (!checked) { out.diagnostics = std::move(checked.diagnostics); return out; }
        unit.program = std::move(*checked);
        out.value = std::move(unit);
    } catch (const std::invalid_argument& e) {
        const auto& w = lowerer.where();
        out.diagnostics.push_back({metachain::Stage::Lower, e.what(), w.file, w.line, w.column, 0});
    }
    return out;
}
Result<metail::Program> ast_to_ir(const AST& ast, Options o) {
    auto unit = lower(ast, std::move(o));
    if (!unit) return {std::nullopt, std::move(unit.diagnostics)};
    return {std::move(unit->program), {}};
}
} // namespace qscript
