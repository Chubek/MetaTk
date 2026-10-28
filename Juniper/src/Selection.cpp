#include "Internal.hpp"
#include <algorithm>
#include <limits>
#include <unordered_map>

namespace juniper {
namespace {
struct SelectionFailure { Diagnostic diagnostic; };
struct Leaf { const Expr* expr; bool immediate; };
struct Plan {
    std::uint64_t cost = std::numeric_limits<std::uint64_t>::max();
    const Rule* rule = nullptr;
    std::vector<Leaf> leaves;
};
class Selector {
    const Backend& backend_;
    const std::vector<ExternalFunction>& externals_;
    const std::unordered_map<std::string, std::size_t>& import_indices_;
    MachineFunction function_;
    std::unordered_map<const Expr*, Plan> plans_;
    std::size_t temporary_ = 0, label_ = 0, statement_ = 0;
    std::uint64_t attempts_ = 0;
    [[noreturn]] void fail(std::string message) const {
        throw SelectionFailure{detail::error("JUN-E300", std::move(message), function_.name)};
    }
    bool match(const Pattern& p, const Expr& e, std::vector<Leaf>& leaves, std::uint64_t& cost) {
        if (++attempts_ > 20000000) fail("selection work budget exceeded (20000000 pattern nodes)");
        if (p.kind == Pattern::Kind::reg) {
            const auto& child = plan(e);
            if (child.cost == std::numeric_limits<std::uint64_t>::max()) return false;
            cost += child.cost; leaves.push_back({&e, false}); return true;
        }
        if (p.kind == Pattern::Kind::immediate) {
            if (e.kind != METAIL_INT || e.value < p.minimum || e.value > p.maximum) return false;
            leaves.push_back({&e, true}); return true;
        }
        if (e.kind != METAIL_OP || p.op != e.name || p.args.size() != e.args.size()) return false;
        for (std::size_t i = 0; i < p.args.size(); ++i)
            if (!match(p.args[i], e.args[i], leaves, cost)) return false;
        return true;
    }
    const Plan& plan(const Expr& e) {
        if (auto found = plans_.find(&e); found != plans_.end()) return found->second;
        Plan best;
        if (e.kind == METAIL_VAR) best.cost = 0;
        else if (e.kind == METAIL_CALL || (e.kind == METAIL_OP && e.name == "select")) {
            best.cost = 1;
            for (const auto& a : e.args) {
                auto cost = plan(a).cost;
                if (cost == std::numeric_limits<std::uint64_t>::max()) { best.cost = cost; break; }
                best.cost += cost;
            }
        } else {
            for (const auto& rule : backend_.rules) {
                Plan candidate; candidate.cost = rule.cost; candidate.rule = &rule;
                if (match(rule.pattern, e, candidate.leaves, candidate.cost) && candidate.cost < best.cost)
                    best = std::move(candidate);
            }
        }
        return plans_.emplace(&e, std::move(best)).first->second;
    }
    std::string temp() { return "$v" + std::to_string(temporary_++); }
    static Operand reg(std::string name) { return {Operand::Kind::reg, std::move(name), 0}; }
    static Operand label(std::string name) { return {Operand::Kind::label, std::move(name), 0}; }
    void emit(std::string opcode, std::string result, std::vector<Operand> operands,
              bool barrier = false, std::uint32_t latency = 1, std::string resource = "alu") {
        if (function_.instructions.size() >= 200000) fail("machine instruction budget exceeded");
        Instruction i;
        i.id = function_.instructions.size(); i.statement = statement_; i.opcode = std::move(opcode);
        i.result = std::move(result); i.operands = std::move(operands); i.barrier = barrier;
        i.latency = latency; i.resource = std::move(resource);
        function_.instructions.push_back(std::move(i));
    }
    static bool observable(const Expr& e) {
        if (e.kind == METAIL_CALL) return true;
        if (e.kind == METAIL_OP && (e.name == "div" || e.name == "mod" || e.name == "shl" || e.name == "shr")) return true;
        return std::any_of(e.args.begin(), e.args.end(), observable);
    }
    Operand expression(const Expr& e) {
        const Plan& selected = plan(e);
        if (selected.cost == std::numeric_limits<std::uint64_t>::max())
            fail("no legal selection for " + (e.kind == METAIL_INT ? std::string("literal ") + std::to_string(e.value) : e.name) +
                " at statement " + std::to_string(statement_ + 1));
        if (e.kind == METAIL_VAR) return reg(e.name);
        std::string result = temp();
        if (e.kind == METAIL_OP && e.name == "select") {
            // Both arms are compiled, but only one executes. Never hoist an arm
            // before the branch, even when its cost would make that attractive.
            auto condition = expression(e.args[0]);
            auto prefix = "$select" + std::to_string(label_++);
            auto yes = prefix + "_yes", no = prefix + "_no", done = prefix + "_done";
            emit("BR", {}, {condition, label(yes), label(no)}, true, 1, "control");
            emit("LABEL", {}, {label(yes)}, true, 1, "control");
            auto a = expression(e.args[1]); emit("COPY", result, {a});
            emit("JUMP", {}, {label(done)}, true, 1, "control");
            emit("LABEL", {}, {label(no)}, true, 1, "control");
            auto b = expression(e.args[2]); emit("COPY", result, {b});
            emit("LABEL", {}, {label(done)}, true, 1, "control");
        } else if (e.kind == METAIL_CALL) {
            std::vector<Operand> args{{Operand::Kind::symbol, e.name, 0}};
            std::optional<std::size_t> external;
            if (auto found = import_indices_.find(e.name); found != import_indices_.end()) {
                external = found->second; args[0].name = externals_[*external].symbol;
            }
            for (const auto& arg : e.args) args.push_back(expression(arg));
            const bool is_void = external && externals_[*external].result == "void";
            emit("CALL", is_void ? std::string{} : result, std::move(args), true, 1, "control");
            function_.instructions.back().external = external;
        } else {
            std::vector<Operand> operands;
            for (const auto& leaf : selected.leaves) {
                if (leaf.immediate) operands.push_back({Operand::Kind::immediate, {}, leaf.expr->value});
                else operands.push_back(expression(*leaf.expr));
            }
            emit(selected.rule->opcode, result, std::move(operands), observable(e),
                 selected.rule->latency, selected.rule->resource);
        }
        return reg(std::move(result));
    }
public:
    Selector(const Backend& b, const Function& f, const std::vector<ExternalFunction>& externals,
             const std::unordered_map<std::string, std::size_t>& indices)
        : backend_(b), externals_(externals), import_indices_(indices) { function_.name = f.name; function_.params = f.params; }
    MachineFunction run(const Function& f) {
        for (const auto& s : f.statements) {
            switch (s.kind) {
            case METAIL_LET: { auto value = expression(s.expr); emit("COPY", s.name, {value}); break; }
            case METAIL_RETURN: { auto value = expression(s.expr); emit("RET", {}, {value}, true, 1, "control"); break; }
            case METAIL_EVAL: (void)expression(s.expr); break;
            case METAIL_ASSUME: { auto value = expression(s.expr); emit("ASSUME", {}, {value}, true, 1, "control"); break; }
            case METAIL_LABEL: emit("LABEL", {}, {label(s.name)}, true, 1, "control"); break;
            case METAIL_GOTO: emit("JUMP", {}, {label(s.target)}, true, 1, "control"); break;
            case METAIL_BRANCH: { auto value = expression(s.expr); emit("BR", {}, {value, label(s.yes), label(s.no)}, true, 1, "control"); break; }
            }
            ++statement_;
        }
        return std::move(function_);
    }
};
} // namespace
Result<MachineModule> select(const Module& m, const char* home) {
    Compiler compiler(home);
    auto valid = compiler.check(m); if (!valid) return *valid.error();
    try {
        MachineModule out; out.name = m.name; out.backend = m.backend;
        out.externals = m.externals;
        std::unordered_map<std::string, std::size_t> indices;
        for (std::size_t i = 0; i < m.externals.size(); ++i) indices.emplace(m.externals[i].name, i);
        for (const auto& f : m.functions) out.functions.push_back(Selector(m.backend, f, m.externals, indices).run(f));
        return out;
    } catch (const SelectionFailure& f) { return f.diagnostic; }
}
} // namespace juniper
