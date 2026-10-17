// QScript type checking and lowering to MetaIL.
//
// Register convention (per function):
//   r0..r15   variables and parameters (block-scoped allocation)
//   r16..r29  expression temporaries; an expression evaluated into t may use
//             t+1.. as scratch
//   r30       return value of the most recent call
//   r31       scratch used only by the assert trap and the call prologue
//
// Calls: the caller pushes its live variables and in-use temporaries, then
// each argument, then `call`s. The callee pops its return address into r31,
// pops its arguments into r0..r(n-1), pushes the return address back, and
// returns with the value in r30. The caller restores its registers and copies
// r30 into its target. Built-in operations are stubs whose body is `host`, so
// they follow the same convention.
//
// Typing: infer() computes an expression's type without emitting code, and
// every node is checked before it is emitted. expr() emits code for a node
// it has already checked, so each operation is emitted for the types of its
// operands.
//
// Layout: the program's top-level statements come first and end in `halt`;
// functions follow, then the built-in stubs they call. A failed `assert`
// executes `const r31, 0; div r31, r31, r31`, which the runtime reports as an
// assertion failure (see Compiler.cpp).
#include "qscript/QScript.h"
#include "qscript/src/Analysis.h"
#include "qscript/src/Runtime.h"
#include <deque>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace qscript {
namespace {
using metail::Op;
using detail::Analysis;
using detail::Builtin;
using detail::BuiltinCall;
using detail::Type;
using dsl::ASTNode;

// Stub names for built-ins cannot collide with qualified QScript names.
std::string builtin_stub_name(std::int64_t id) { return "\x01" + std::to_string(id); }

std::string plural(std::size_t n, const char* noun) {
    return std::to_string(n) + " " + noun + (n == 1 ? "" : "s");
}

// A call target: a user function, or a built-in reached through its stub.
struct Callee {
    std::string stub;                      // label the call jumps to
    std::optional<std::int64_t> host;      // set when the stub is a `host` stub
    std::vector<Type> params;              // receiver first for methods
    std::optional<Type> type_arg;          // type descriptor pushed after the arguments
    Type result;
    std::string display;                   // for diagnostics
    std::size_t arity() const { return params.size() + (type_arg ? 1 : 0); }
};

// An argument: an expression, a constant, or a value already in a register.
struct Arg {
    enum class Kind { Node, Constant, Register };
    Kind kind = Kind::Node;
    const ASTNode* node = nullptr;
    std::int64_t value = 0;
    unsigned source = 0;
    static Arg of(const ASTNode& n) { Arg a; a.node = &n; return a; }
    static Arg constant(std::int64_t v) { Arg a; a.kind = Kind::Constant; a.value = v; return a; }
    static Arg from_register(unsigned r) { Arg a; a.kind = Kind::Register; a.source = r; return a; }
};

bool is_comparison(const std::string& tag) {
    return tag == "==" || tag == "!=" || tag == "<" || tag == ">" || tag == "<=" || tag == ">=";
}

class Lowerer {
    const Options& o_;
    const Analysis& a_;
    Unit unit_;
    metail::Program& p_;
    metavm::SourceLocation where_;
    struct Fixup { std::size_t pc; std::string target; std::string owner; };  // owner: "" for top-level code
    std::vector<Fixup> calls_;
    std::map<std::string, FunctionInfo> infos_;   // metadata of emitted functions
    std::map<std::string, std::size_t> entries_;
    struct Stub { std::int64_t host; std::size_t arity; };
    std::map<std::string, Stub> stubs_;
    std::map<std::string, std::size_t> string_index_, type_index_, grammar_index_;
    std::deque<ASTNode> synthetic_;        // nodes built during lowering (stable references)
    std::size_t hidden_ = 0;
    std::string fn_name_, fn_unit_;
    Type fn_result_;
    bool in_function_ = false;
    // Integers print through the VM's integer channel (Op::Print) unless the
    // program also prints other values; then every print goes through the
    // session so that lines keep their order (see lower()).
    bool host_prints_ = false;
    bool text_print_ = false;
    struct Var { unsigned reg; Type type; };
    std::vector<std::map<std::string, Var>> scopes_;
    std::set<unsigned> free_;
    struct Loop { std::vector<std::size_t> breaks, continues; };
    std::vector<Loop> loops_;

    // ---- basics ----------------------------------------------------------
    static std::uint8_t reg(unsigned n) { return static_cast<std::uint8_t>(n); }
    [[noreturn]] static void fail(const std::string& message) { throw std::invalid_argument(message); }
    static void require(const Type& got, const Type& want, const std::string& what) {
        if (got != want) fail(what + ": expected " + want.str() + ", got " + got.str());
    }
    std::size_t emit(metail::Instruction i) {
        if (p_.size() >= o_.toolchain.il.max_instructions) fail("program is too large");
        p_.code.push_back(i);
        p_.source_map.push_back(where_);
        return p_.size() - 1;
    }
    std::size_t here() const { return p_.size(); }
    void patch(std::size_t pc, std::size_t target) { p_.code[pc].immediate = static_cast<std::int64_t>(target); }
    void locate(const ASTNode& n) {
        const auto l = detail::location(n);
        if (l.line) where_ = {l.file, static_cast<std::uint32_t>(l.line), static_cast<std::uint32_t>(l.column)};
    }
    static void check_temporary(unsigned t) {
        if (t > detail::last_temporary) fail("expression is nested too deeply; split it with let");
    }
    const ASTNode& synth(ASTNode n) {
        synthetic_.push_back(std::move(n));
        return synthetic_.back();
    }
    std::string hidden_name(const char* stem) { return std::string("$") + stem + std::to_string(++hidden_); }
    static std::vector<const ASTNode*> args_of(const ASTNode& n, std::size_t first) {
        std::vector<const ASTNode*> out;
        for (std::size_t i = first; i < detail::arity(n); ++i) out.push_back(&n.child(i));
        return out;
    }
    static std::vector<Arg> args_as(const std::vector<const ASTNode*>& nodes) {
        std::vector<Arg> out;
        for (const auto* n : nodes) out.push_back(Arg::of(*n));
        return out;
    }

    // ---- scopes and names -----------------------------------------------
    void reset(const std::string& unit, bool in_function) {
        fn_unit_ = unit;
        in_function_ = in_function;
        scopes_.clear();
        loops_.clear();
        free_.clear();
        for (unsigned r = 0; r < detail::max_variables; ++r) free_.insert(r);
    }
    void enter() { scopes_.emplace_back(); }
    void leave() {
        for (const auto& [name, v] : scopes_.back()) { (void)name; free_.insert(v.reg); }
        scopes_.pop_back();
    }
    unsigned allocate(const std::string& name, const Type& type) {
        if (free_.empty()) fail("more than 16 variables are live at once");
        const unsigned r = *free_.begin();
        free_.erase(free_.begin());
        scopes_.back()[name] = Var{r, type};
        return r;
    }
    const Var* variable(const std::string& name) const {
        for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it)
            if (auto f = it->find(name); f != it->end()) return &f->second;
        return nullptr;
    }
    std::set<unsigned> live_variables() const {
        std::set<unsigned> r;
        for (const auto& s : scopes_) for (const auto& [name, v] : s) { (void)name; r.insert(v.reg); }
        return r;
    }
    std::string constant_key(const std::string& name) const {
        std::string why;
        return a_.constant(fn_unit_, name, why);
    }
    std::string grammar_key(const std::string& name) const {
        std::string why;
        return a_.grammar(fn_unit_, name, why);
    }
    // Name lookup order: variable, constant, grammar.
    Type name_type(const std::string& name) const {
        if (const auto* v = variable(name)) return v->type;
        if (!constant_key(name).empty()) return Type::integer();
        if (!grammar_key(name).empty()) return Type::grammar();
        fail("undefined variable: " + name);
    }
    std::int64_t constant_value(const std::string& name) const {
        const auto q = constant_key(name);
        auto it = a_.constants.find(q);
        if (q.empty() || it == a_.constants.end() || !it->second.value) fail("invalid constant: " + name);
        return *it->second.value;
    }

    // ---- constant tables ---------------------------------------------------
    std::size_t string_const(const std::string& s) {
        auto [it, inserted] = string_index_.emplace(s, unit_.strings.size());
        if (inserted) unit_.strings.push_back(s);
        return it->second;
    }
    std::size_t type_const(const Type& t) {
        auto [it, inserted] = type_index_.emplace(t.str(), unit_.types.size());
        if (inserted) unit_.types.push_back(t.str());
        return it->second;
    }
    std::size_t grammar_const(const std::string& key) {
        if (auto it = grammar_index_.find(key); it != grammar_index_.end()) return it->second;
        const auto& g = a_.grammars.at(key);
        if (!g.compiled) fail("invalid grammar " + (g.name.empty() ? std::string("literal") : g.name));
        const std::size_t index = unit_.grammars.size();
        unit_.grammars.push_back(GrammarInfo{g.name.empty() ? std::string("<grammar literal>") : g.name, g.source, g.compiled});
        grammar_index_[key] = index;
        return index;
    }

    // ---- callees -----------------------------------------------------------
    static Callee builtin_callee(const BuiltinCall& b, std::string display) {
        const auto id = detail::builtin_id(b.id);
        return Callee{builtin_stub_name(id), id, b.params, b.type_arg, b.result, std::move(display)};
    }
    static Callee sys(Builtin b, std::vector<Type> params, Type result, std::optional<Type> type_arg, std::string display) {
        return builtin_callee(BuiltinCall{b, std::move(params), std::move(result), std::move(type_arg)}, std::move(display));
    }
    void request(const std::string& name, std::int64_t host, std::size_t arity) {
        stubs_.emplace(name, Stub{host, arity});
    }

    // The function or built-in named by a `call` node.
    Callee resolve_call(const ASTNode& n) {
        locate(n);
        const std::string name = n.child(0).value();
        const std::size_t argc = detail::arity(n) - 1;
        std::string why;
        const auto q = a_.function(fn_unit_, name, why);
        if (!q.empty()) {
            const auto& f = a_.functions.at(q);
            if (f.params.size() != argc)
                fail(name + "() expects " + plural(f.params.size(), "argument") + ", got " + std::to_string(argc));
            return Callee{q, f.host_id, f.param_types, std::nullopt, f.result, name + "()"};
        }
        if (auto b = detail::resolve_global(name, argc)) return builtin_callee(*b, name + "()");
        fail(why.empty() ? "undefined function: " + name : why);
    }
    // The built-in implementing `recv.name(...)`.
    Callee resolve_method_call(const ASTNode& n, const Type& recv) {
        locate(n);
        const std::string name = n.child(1).value();
        std::string why;
        auto b = detail::resolve_method(recv, name, detail::arity(n) - 2, why);
        if (!b) fail(why);
        return builtin_callee(*b, "." + name + "()");
    }
    void check_args(const Callee& c, const std::vector<const ASTNode*>& args, std::size_t first) {
        for (std::size_t i = 0; i < args.size(); ++i) {
            const Type got = infer(*args[i], &c.params[first + i]);
            require(got, c.params[first + i], "argument " + std::to_string(i + 1) + " of " + c.display);
        }
    }

    // ---- typing (no code) ---------------------------------------------------
    Type infer(const ASTNode& n, const Type* expect = nullptr) {
        const std::string tag(n.tag());
        if (n.is_leaf()) {
            if (tag == "integer") return Type::integer();
            if (tag == "string") return Type::string();
            if (tag == "name") return name_type(n.value());
            fail("invalid expression: " + tag);
        }
        if (tag == "call") {
            const Callee c = resolve_call(n);
            check_args(c, args_of(n, 1), 0);
            return c.result;
        }
        if (tag == "method") {
            const Type recv = infer(n.child(0));
            const Callee c = resolve_method_call(n, recv);
            check_args(c, args_of(n, 2), 1);
            return c.result;
        }
        if (tag == "index") {
            const Type base = infer(n.child(0));
            if (!base.is_list()) fail("cannot index a value of type " + base.str() + " (only lists can be indexed)");
            require(infer(n.child(1)), Type::integer(), "list index");
            return base.element();
        }
        if (tag == "list") {
            const std::size_t count = detail::arity(n);
            const Type* elem_expect = expect && expect->is_list() ? &expect->element() : nullptr;
            if (count == 0) {
                if (expect && expect->is_list()) return *expect;
                fail("cannot infer the element type of []; annotate it, e.g. let xs: list<int> = []");
            }
            const Type e = infer(n.child(0), elem_expect);
            if (elem_expect) require(e, *elem_expect, "list element");
            for (std::size_t i = 1; i < count; ++i) require(infer(n.child(i), &e), e, "list element");
            return Type::list(e);
        }
        if (tag == "grammar-lit") return Type::grammar();
        if (tag == "neg" || tag == "not" || tag == "invert") {
            const std::string op = tag == "neg" ? "-" : tag == "not" ? "!" : "~";
            require(infer(n.child(0)), Type::integer(), "operand of " + op);
            return Type::integer();
        }
        if (tag == "&&" || tag == "||") {
            require(infer(n.child(0)), Type::integer(), "operand of " + tag);
            require(infer(n.child(1)), Type::integer(), "operand of " + tag);
            return Type::integer();
        }
        if (tag == "?:") {
            require(infer(n.child(0)), Type::integer(), "condition of ?:");
            const Type a = infer(n.child(1), expect);
            const Type b = infer(n.child(2), &a);
            if (a != b) fail("branches of ?: have different types: " + a.str() + " and " + b.str());
            return a;
        }
        if (tag == "~") {
            require(infer(n.child(0)), Type::string(), "left operand of ~");
            require(infer(n.child(1)), Type::string(), "right operand of ~");
            return Type::string();
        }
        if (is_comparison(tag)) {
            const Type a = infer(n.child(0));
            const Type b = infer(n.child(1), &a);
            if (a != b || !(a.is_int() || a.is_str()))
                fail("cannot compare " + a.str() + " with " + b.str() + " using " + tag);
            return Type::integer();
        }
        static const std::set<std::string> arithmetic{"+", "-", "*", "/", "%", "&", "|", "^", "<<", ">>", "**"};
        if (arithmetic.count(tag)) {
            require(infer(n.child(0)), Type::integer(), "left operand of " + tag);
            require(infer(n.child(1)), Type::integer(), "right operand of " + tag);
            return Type::integer();
        }
        fail("invalid expression: " + tag);
    }

    // ---- emission primitives -----------------------------------------------
    // Evaluate one argument into register t; returns its type.
    Type eval_arg(const Arg& a, unsigned t, const Type& want) {
        switch (a.kind) {
        case Arg::Kind::Node: return expr(*a.node, t, &want);
        case Arg::Kind::Constant: emit({Op::Const, reg(t), 0, 0, a.value}); return Type::integer();
        case Arg::Kind::Register: emit({Op::Mov, reg(t), reg(a.source), 0, 0}); return want;
        }
        fail("internal: invalid argument");
    }
    // Emit a call with the result in register t.
    void emit_call(const Callee& c, const std::vector<Arg>& args, unsigned t) {
        check_temporary(t);
        if (args.size() != c.params.size())
            fail(c.display + " expects " + plural(c.params.size(), "argument") + ", got " + std::to_string(args.size()));
        if (c.host) request(c.stub, *c.host, c.arity());
        std::vector<unsigned> saved;
        for (auto v : live_variables()) saved.push_back(v);
        for (unsigned r = detail::first_temporary; r < t; ++r) saved.push_back(r);
        for (auto r : saved) emit({Op::Push, reg(r), 0, 0, 0});
        for (std::size_t i = 0; i < args.size(); ++i) {
            const Type got = eval_arg(args[i], t, c.params[i]);
            require(got, c.params[i], "argument " + std::to_string(i + 1) + " of " + c.display);
            emit({Op::Push, reg(t), 0, 0, 0});
        }
        if (c.type_arg) {
            emit({Op::Const, reg(t), 0, 0, static_cast<std::int64_t>(type_const(*c.type_arg))});
            emit({Op::Push, reg(t), 0, 0, 0});
        }
        calls_.push_back({emit({Op::Call, 0, 0, 0, 0}), c.stub, fn_name_});
        for (auto it = saved.rbegin(); it != saved.rend(); ++it) emit({Op::Pop, reg(*it), 0, 0, 0});
        emit({Op::Mov, reg(t), reg(detail::return_register), 0, 0});
    }
    // t = (t != 0)
    void normalize(unsigned t) {
        emit({Op::Const, reg(t + 1), 0, 0, 0});
        emit({Op::Eq, reg(t), reg(t), reg(t + 1), 0});
        emit({Op::Const, reg(t + 1), 0, 0, 0});
        emit({Op::Eq, reg(t), reg(t), reg(t + 1), 0});
    }
    // t = !t, using u as scratch.
    void invert(unsigned t, unsigned u) {
        emit({Op::Const, reg(u), 0, 0, 0});
        emit({Op::Eq, reg(t), reg(t), reg(u), 0});
    }
    // t = (t tag u) for integers.
    void emit_compare(const std::string& tag, unsigned t, unsigned u) {
        if (tag == "==") emit({Op::Eq, reg(t), reg(t), reg(u), 0});
        else if (tag == "!=") { emit({Op::Eq, reg(t), reg(t), reg(u), 0}); invert(t, u); }
        else if (tag == "<") emit({Op::Lt, reg(t), reg(t), reg(u), 0});
        else if (tag == ">") emit({Op::Lt, reg(t), reg(u), reg(t), 0});
        else if (tag == "<=") { emit({Op::Lt, reg(t), reg(u), reg(t), 0}); invert(t, u); }
        else { emit({Op::Lt, reg(t), reg(t), reg(u), 0}); invert(t, u); }   // >=
    }

    // ---- expressions ---------------------------------------------------------
    // Emit `n` into register t. The node is checked first; the returned type
    // is the one infer() gives.
    Type expr(const ASTNode& n, unsigned t, const Type* expect = nullptr) {
        check_temporary(t);
        const Type result = infer(n, expect);
        const std::string tag(n.tag());
        if (n.is_leaf()) {
            if (tag == "integer") {
                emit({Op::Const, reg(t), 0, 0, *metachain::text::signed_integer(n.value())});
            } else if (tag == "string") {
                emit_call(sys(Builtin::StrLiteral, {Type::integer()}, Type::string(), std::nullopt, "string literal"),
                          {Arg::constant(static_cast<std::int64_t>(string_const(n.value())))}, t);
            } else if (const auto* v = variable(n.value())) {
                emit({Op::Mov, reg(t), reg(v->reg), 0, 0});
            } else if (!constant_key(n.value()).empty()) {
                emit({Op::Const, reg(t), 0, 0, constant_value(n.value())});
            } else {
                const auto key = grammar_key(n.value());
                emit_call(sys(Builtin::GrammarRef, {Type::integer()}, Type::grammar(), std::nullopt, "grammar " + n.value()),
                          {Arg::constant(static_cast<std::int64_t>(grammar_const(key)))}, t);
            }
            return result;
        }
        if (tag == "call") {
            emit_call(resolve_call(n), args_as(args_of(n, 1)), t);
            return result;
        }
        if (tag == "method") {
            const Type recv = infer(n.child(0));
            const Callee c = resolve_method_call(n, recv);
            std::vector<Arg> args{Arg::of(n.child(0))};
            for (auto a : args_as(args_of(n, 2))) args.push_back(a);
            emit_call(c, args, t);
            return result;
        }
        if (tag == "index") {
            const Type base = infer(n.child(0));
            emit_call(sys(Builtin::ListGet, {base, Type::integer()}, base.element(), std::nullopt, "index"),
                      {Arg::of(n.child(0)), Arg::of(n.child(1))}, t);
            return result;
        }
        if (tag == "list") return list_literal(n, t, result);
        if (tag == "grammar-lit") {
            auto it = a_.grammar_literals.find(&n);
            if (it == a_.grammar_literals.end()) fail("internal: unknown grammar literal");
            emit_call(sys(Builtin::GrammarRef, {Type::integer()}, Type::grammar(), std::nullopt, "grammar literal"),
                      {Arg::constant(static_cast<std::int64_t>(grammar_const(it->second)))}, t);
            return result;
        }
        if (tag == "neg") {
            expr(n.child(0), t);
            emit({Op::Const, reg(t + 1), 0, 0, 0});
            emit({Op::Sub, reg(t), reg(t + 1), reg(t), 0});
            return result;
        }
        if (tag == "not") {
            expr(n.child(0), t);
            emit({Op::Const, reg(t + 1), 0, 0, 0});
            emit({Op::Eq, reg(t), reg(t), reg(t + 1), 0});
            return result;
        }
        if (tag == "invert") {
            expr(n.child(0), t);
            emit({Op::Const, reg(t + 1), 0, 0, -1});
            emit({Op::Xor, reg(t), reg(t), reg(t + 1), 0});
            return result;
        }
        if (tag == "&&") {
            expr(n.child(0), t);
            const auto skip = emit({Op::Jz, reg(t), 0, 0, 0});
            expr(n.child(1), t);
            normalize(t);
            patch(skip, here());
            return result;
        }
        if (tag == "||") {
            expr(n.child(0), t);
            const auto rhs = emit({Op::Jz, reg(t), 0, 0, 0});
            emit({Op::Const, reg(t), 0, 0, 1});
            const auto end = emit({Op::Jmp, 0, 0, 0, 0});
            patch(rhs, here());
            expr(n.child(1), t);
            normalize(t);
            patch(end, here());
            return result;
        }
        if (tag == "?:") {
            expr(n.child(0), t);
            const auto otherwise = emit({Op::Jz, reg(t), 0, 0, 0});
            expr(n.child(1), t, &result);
            const auto end = emit({Op::Jmp, 0, 0, 0, 0});
            patch(otherwise, here());
            expr(n.child(2), t, &result);
            patch(end, here());
            return result;
        }
        if (tag == "~") {
            emit_call(sys(Builtin::StrConcat, {Type::string(), Type::string()}, Type::string(), std::nullopt, "~"),
                      {Arg::of(n.child(0)), Arg::of(n.child(1))}, t);
            return result;
        }
        if (is_comparison(tag)) {
            const Type a = infer(n.child(0));
            if (a.is_str()) {
                emit_call(sys(Builtin::StrCompare, {Type::string(), Type::string()}, Type::integer(), std::nullopt, "string comparison"),
                          {Arg::of(n.child(0)), Arg::of(n.child(1))}, t);
                check_temporary(t + 1);
                emit({Op::Const, reg(t + 1), 0, 0, 0});
                emit_compare(tag, t, t + 1);
            } else {
                expr(n.child(0), t);
                expr(n.child(1), t + 1);
                emit_compare(tag, t, t + 1);
            }
            return result;
        }
        if (tag == "**") {
            const auto it = a_.functions.find(std::string(detail::core_unit) + "::pow");
            if (it == a_.functions.end()) fail("internal: the power operator needs the core library");
            const auto& f = it->second;
            const Callee c{it->first, f.host_id, f.param_types, std::nullopt, f.result, "**"};
            emit_call(c, {Arg::of(n.child(0)), Arg::of(n.child(1))}, t);
            return result;
        }
        static const std::map<std::string, Op> ops{{"+", Op::Add}, {"-", Op::Sub}, {"*", Op::Mul}, {"/", Op::Div},
            {"%", Op::Mod}, {"&", Op::And}, {"|", Op::Or}, {"^", Op::Xor}, {"<<", Op::Shl}, {">>", Op::Shr}};
        const auto op = ops.find(tag);
        if (op == ops.end()) fail("invalid expression: " + tag);
        expr(n.child(0), t);
        expr(n.child(1), t + 1);
        emit({op->second, reg(t), reg(t), reg(t + 1), 0});
        return result;
    }
    Type list_literal(const ASTNode& n, unsigned t, const Type& lt) {
        const Type e = lt.element();
        emit_call(sys(Builtin::ListNew, {}, lt, std::nullopt, "list literal"), {}, t);
        const std::size_t count = detail::arity(n);
        for (std::size_t i = 0; i < count; ++i) {
            check_temporary(t + 1);
            emit_call(sys(Builtin::ListPush, {lt, e}, Type::integer(), std::nullopt, "list literal"),
                      {Arg::from_register(t), Arg::of(n.child(i))}, t + 1);
        }
        return lt;
    }

    // ---- statements ----------------------------------------------------------
    void scoped(const ASTNode& n) { enter(); stmt(n); leave(); }
    void close_loop(std::size_t break_target, std::size_t continue_target) {
        auto loop = std::move(loops_.back());
        loops_.pop_back();
        for (auto pc : loop.breaks) patch(pc, break_target);
        for (auto pc : loop.continues) patch(pc, continue_target);
    }
    // base[index] = value
    void list_store(const ASTNode& base, const ASTNode& index, const ASTNode& value, unsigned t) {
        const Type bt = infer(base);
        if (!bt.is_list()) fail("cannot assign to an index of " + bt.str() + " (only lists can be indexed)");
        require(infer(index), Type::integer(), "list index");
        const Type elem = bt.element();
        require(infer(value, &elem), elem, "element assignment");
        emit_call(sys(Builtin::ListSet, {bt, Type::integer(), elem}, Type::integer(), std::nullopt, "list element assignment"),
                  {Arg::of(base), Arg::of(index), Arg::of(value)}, t);
    }
    void foreach_stmt(const ASTNode& n) {
        const unsigned t = detail::first_temporary;
        const std::string var = n.child(0).value();
        if (!constant_key(var).empty()) fail(var + " is a constant");
        const Type lt = infer(n.child(1));
        if (!lt.is_list()) fail("foreach needs a list, not " + lt.str());
        const Type elem = lt.element();
        enter();
        const unsigned list_reg = allocate(hidden_name("list"), lt);
        expr(n.child(1), t);
        emit({Op::Mov, reg(list_reg), reg(t), 0, 0});
        const unsigned index = allocate(hidden_name("i"), Type::integer());
        emit({Op::Const, reg(index), 0, 0, 0});
        const auto start = here();
        emit_call(sys(Builtin::ListLen, {lt}, Type::integer(), std::nullopt, "foreach"),
                  {Arg::from_register(list_reg)}, t);
        emit({Op::Lt, reg(t), reg(index), reg(t), 0});
        const auto exit = emit({Op::Jz, reg(t), 0, 0, 0});
        enter();
        const unsigned element = allocate(var, elem);
        emit_call(sys(Builtin::ListGet, {lt, Type::integer()}, elem, std::nullopt, "foreach"),
                  {Arg::from_register(list_reg), Arg::from_register(index)}, t);
        emit({Op::Mov, reg(element), reg(t), 0, 0});
        loops_.emplace_back();
        stmt(n.child(2));
        leave();
        const auto step = here();
        emit({Op::Const, reg(t), 0, 0, 1});
        emit({Op::Add, reg(index), reg(index), reg(t), 0});
        emit({Op::Jmp, 0, 0, 0, static_cast<std::int64_t>(start)});
        patch(exit, here());
        close_loop(here(), step);
        leave();
    }
    void stmt(const ASTNode& n) {
        locate(n);
        const std::string tag(n.tag());
        const unsigned t = detail::first_temporary;
        if (tag == "block") {
            enter();
            for (std::size_t i = 0; i < detail::arity(n); ++i) stmt(n.child(i));
            leave();
        } else if (tag == "let") {
            const std::string name = n.child(0).value();
            std::optional<Type> annotated;
            const std::string spelling = n.child(1).value();
            if (!spelling.empty()) {
                auto parsed = detail::parse_type(spelling);
                if (!parsed) fail("unknown type " + spelling);
                annotated = *parsed;
            }
            // Shadowing an outer block's variable is allowed; redeclaring one
            // in the same block is not.
            if (scopes_.back().count(name)) fail("variable already defined: " + name);
            if (!constant_key(name).empty()) fail(name + " is a constant");
            const Type* want = annotated ? &*annotated : nullptr;
            const Type declared = annotated ? *annotated : infer(n.child(2));
            if (annotated) require(infer(n.child(2), want), *annotated, "initial value of " + name);
            expr(n.child(2), t, want);
            const unsigned r = allocate(name, declared);
            emit({Op::Mov, reg(r), reg(t), 0, 0});
        } else if (tag == "assign") {
            const std::string name = n.child(0).value();
            const Var* v = variable(name);
            if (!v) {
                if (!constant_key(name).empty()) fail("cannot assign to constant " + name);
                fail("assignment to undefined variable: " + name);
            }
            const Var target = *v;
            const Type vt = expr(n.child(1), t, &target.type);
            require(vt, target.type, "assignment to " + name);
            emit({Op::Mov, reg(target.reg), reg(t), 0, 0});
        } else if (tag == "index-assign") {
            list_store(n.child(0), n.child(1), n.child(2), t);
        } else if (tag == "index-update") {
            // base[i] op= v: the base and the index are each evaluated once,
            // into hidden variables, when they are not plain names.
            enter();
            const ASTNode* base = &n.child(0);
            if (!n.child(0).is_leaf()) {
                const Type bt = infer(n.child(0));
                if (!bt.is_list()) fail("cannot assign to an index of " + bt.str() + " (only lists can be indexed)");
                const std::string b = hidden_name("base");
                const unsigned breg = allocate(b, bt);
                expr(n.child(0), t);
                emit({Op::Mov, reg(breg), reg(t), 0, 0});
                base = &synth(ASTNode("name", b));
            }
            const std::string k = hidden_name("k");
            require(infer(n.child(1)), Type::integer(), "list index");
            const unsigned kreg = allocate(k, Type::integer());
            expr(n.child(1), t);
            emit({Op::Mov, reg(kreg), reg(t), 0, 0});
            const ASTNode& kname = synth(ASTNode("name", k));
            const ASTNode& read = synth(ASTNode("index", std::vector<ASTNode>{*base, kname}));
            const std::string op(n.child(2).value());
            const ASTNode& value = synth(ASTNode(op, std::vector<ASTNode>{read, n.child(3)}));
            list_store(*base, kname, value, t);
            leave();
        } else if (tag == "expr") {
            expr(n.child(0), t);
        } else if (tag == "print") {
            for (std::size_t i = 0; i < detail::arity(n); ++i) {
                const Type vt = expr(n.child(i), t);
                if (!vt.is_int()) text_print_ = true;
                if (vt.is_int() && !host_prints_) {
                    emit({Op::Print, reg(t), 0, 0, 0});
                } else {
                    emit_call(sys(Builtin::PrintValue, {vt}, Type::integer(), vt, "say"), {Arg::from_register(t)}, t + 1);
                }
            }
        } else if (tag == "return" && in_function_) {
            const Type vt = expr(n.child(0), t, &fn_result_);
            require(vt, fn_result_, "return value of " + fn_name_);
            emit({Op::Mov, reg(detail::return_register), reg(t), 0, 0});
            emit({Op::Ret, 0, 0, 0, 0});
        } else if (tag == "return" || tag == "exit") {
            require(infer(n.child(0)), Type::integer(), "result of " + tag);
            expr(n.child(0), t);
            emit({Op::Halt, reg(t), 0, 0, 0});
        } else if (tag == "assert") {
            require(infer(n.child(0)), Type::integer(), "assert condition");
            expr(n.child(0), t);
            const auto ok = emit({Op::Jnz, reg(t), 0, 0, 0});
            // Recognised by Compiler.cpp: const r31, 0 ; div r31, r31, r31
            emit({Op::Const, 31, 0, 0, 0});
            emit({Op::Div, 31, 31, 31, 0});
            patch(ok, here());
        } else if (tag == "if") {
            require(infer(n.child(0)), Type::integer(), "if condition");
            expr(n.child(0), t);
            const auto branch = emit({Op::Jz, reg(t), 0, 0, 0});
            scoped(n.child(1));
            if (detail::arity(n) == 3) {
                const auto end = emit({Op::Jmp, 0, 0, 0, 0});
                patch(branch, here());
                scoped(n.child(2));
                patch(end, here());
            } else {
                patch(branch, here());
            }
        } else if (tag == "while") {
            const auto start = here();
            require(infer(n.child(0)), Type::integer(), "while condition");
            expr(n.child(0), t);
            const auto exit = emit({Op::Jz, reg(t), 0, 0, 0});
            loops_.emplace_back();
            scoped(n.child(1));
            emit({Op::Jmp, 0, 0, 0, static_cast<std::int64_t>(start)});
            patch(exit, here());
            close_loop(here(), start);
        } else if (tag == "do") {
            const auto start = here();
            loops_.emplace_back();
            scoped(n.child(0));
            const auto test = here();
            require(infer(n.child(1)), Type::integer(), "do-while condition");
            expr(n.child(1), t);
            emit({Op::Jnz, reg(t), 0, 0, static_cast<std::int64_t>(start)});
            close_loop(here(), test);
        } else if (tag == "foreach") {
            foreach_stmt(n);
        } else if (tag == "for") {
            enter();
            stmt(n.child(0));
            const auto start = here();
            require(infer(n.child(1)), Type::integer(), "for condition");
            expr(n.child(1), t);
            const auto exit = emit({Op::Jz, reg(t), 0, 0, 0});
            loops_.emplace_back();
            scoped(n.child(3));
            const auto step = here();
            stmt(n.child(2));
            emit({Op::Jmp, 0, 0, 0, static_cast<std::int64_t>(start)});
            patch(exit, here());
            close_loop(here(), step);
            leave();
        } else if (tag == "break" || tag == "continue") {
            if (loops_.empty()) fail(tag + " outside a loop");
            const auto jump = emit({Op::Jmp, 0, 0, 0, 0});
            (tag == "break" ? loops_.back().breaks : loops_.back().continues).push_back(jump);
        } else {
            fail("invalid statement: " + tag);
        }
    }

    // ---- functions and stubs -----------------------------------------------
    void emit_stub(const std::string& name, const Stub& stub) {
        entries_[name] = here();
        reset("", false);
        enter();
        emit({Op::Pop, reg(detail::scratch_register), 0, 0, 0});
        std::vector<unsigned> params;
        for (std::size_t i = 0; i < stub.arity; ++i) params.push_back(allocate("$arg" + std::to_string(i), Type::integer()));
        for (auto it = params.rbegin(); it != params.rend(); ++it) emit({Op::Pop, reg(*it), 0, 0, 0});
        emit({Op::Push, reg(detail::scratch_register), 0, 0, 0});
        emit({Op::Host, reg(detail::return_register), 0, 0, stub.host});
        emit({Op::Ret, 0, 0, 0, 0});
        leave();
    }
    void emit_function(const std::string& q) {
        const auto& f = a_.functions.at(q);
        reset(f.unit, true);
        fn_name_ = q;
        fn_result_ = f.result;
        if (f.decl) locate(*f.decl); else where_ = {};
        entries_[q] = here();
        enter();
        emit({Op::Pop, reg(detail::scratch_register), 0, 0, 0});
        std::vector<unsigned> params;
        for (std::size_t i = 0; i < f.params.size(); ++i) params.push_back(allocate(f.params[i], f.param_types[i]));
        for (auto it = params.rbegin(); it != params.rend(); ++it) emit({Op::Pop, reg(*it), 0, 0, 0});
        emit({Op::Push, reg(detail::scratch_register), 0, 0, 0});
        stmt(*f.body);
        leave();
        if (f.result.is_int()) {
            emit({Op::Const, reg(detail::return_register), 0, 0, 0});
            emit({Op::Ret, 0, 0, 0, 0});
        } else {
            // Falling off the end of a str/list/grammar/match function traps.
            const unsigned t = detail::first_temporary;
            emit_call(sys(Builtin::StrLiteral, {Type::integer()}, Type::string(), std::nullopt, "trap"),
                      {Arg::constant(static_cast<std::int64_t>(string_const("function " + q + " ended without returning a value")))}, t);
            emit_call(sys(Builtin::Die, {Type::string()}, Type::integer(), std::nullopt, "trap"),
                      {Arg::from_register(t)}, t);
            emit({Op::Halt, reg(detail::return_register), 0, 0, 0});
        }
        const auto l = f.decl ? detail::location(*f.decl) : detail::Location{};
        FunctionInfo info;
        info.name = q;
        info.entry = entries_[q];
        info.arity = f.params.size();
        info.file = l.file;
        info.line = l.line;
        for (const auto& t : f.param_types) info.parameter_types.push_back(t.str());
        info.result_type = f.result.str();
        infos_[q] = std::move(info);
    }

public:
    Lowerer(const Options& o, const Analysis& a, bool host_prints) : o_(o), a_(a), p_(unit_.program), host_prints_(host_prints) {}
    const metavm::SourceLocation& where() const { return where_; }
    bool prints_text() const { return text_print_; }

    Unit run() {
        reset("", false);
        enter();
        for (const auto* s : a_.statements) stmt(*s);
        leave();
        where_ = {};
        emit({Op::Const, reg(detail::first_temporary), 0, 0, 0});
        emit({Op::Halt, reg(detail::first_temporary), 0, 0, 0});
        for (const auto& q : a_.emitted) emit_function(q);
        // Stubs for built-ins and host-backed functions requested so far.
        // Emitting a stub never requests another, so one pass is enough.
        for (const auto& [name, stub] : stubs_) if (!entries_.count(name)) emit_stub(name, stub);
        for (const auto& fix : calls_) {
            auto it = entries_.find(fix.target);
            if (it == entries_.end()) fail("internal: call to a function that was not emitted: " + fix.target);
            patch(fix.pc, it->second);
        }
        // Every function is type-checked and emitted, so library errors are
        // reported. The public list holds the program's own functions and the
        // library functions that its code reaches.
        // Roots: the top-level code and every function the program defines.
        std::set<std::string> reachable;
        std::vector<std::string> work{""};
        for (const auto& q : a_.emitted) if (a_.functions.at(q).unit.empty()) work.push_back(q);
        while (!work.empty()) {
            const auto owner = work.back();
            work.pop_back();
            for (const auto& fix : calls_)
                if (fix.owner == owner && reachable.insert(fix.target).second) work.push_back(fix.target);
        }
        for (const auto& q : a_.emitted) {
            if (a_.functions.at(q).unit.empty() || reachable.count(q)) unit_.functions.push_back(infos_.at(q));
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
    // Programs that print only integers keep the VM's integer output. A
    // program that also prints text is lowered again so that all of its
    // prints go through the session, which keeps the printed lines in order.
    std::optional<Lowerer> lowerer;
    lowerer.emplace(o, analysis, false);
    try {
        Unit unit = lowerer->run();
        if (lowerer->prints_text()) {
            lowerer.emplace(o, analysis, true);
            unit = lowerer->run();
        }
        auto checked = verify_ir(std::move(unit.program), o);
        if (!checked) { out.diagnostics = std::move(checked.diagnostics); return out; }
        unit.program = std::move(*checked);
        out.value = std::move(unit);
    } catch (const std::exception& e) {
        const auto& w = lowerer->where();
        // Type and scope errors are reported as verification errors, the stage
        // the analysis uses for the same kinds of mistake.
        out.diagnostics.push_back({metachain::Stage::Verify, e.what(), w.file, w.line, w.column, 0});
    }
    return out;
}

Result<metail::Program> ast_to_ir(const AST& ast, Options o) {
    auto unit = lower(ast, std::move(o));
    if (!unit) return {std::nullopt, std::move(unit.diagnostics)};
    return {std::move(unit->program), {}};
}

std::vector<Diagnostic> sema_check(const AST& ast, Options o) {
    auto unit = lower(ast, std::move(o));
    return unit ? std::vector<Diagnostic>{} : unit.diagnostics;
}
} // namespace qscript
