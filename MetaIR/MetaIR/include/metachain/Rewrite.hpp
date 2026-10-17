#pragma once
#ifndef REWRITE_HPP_INCLUDED
#define REWRITE_HPP_INCLUDED

// Rewrite.hpp — header-only C++20 term rewriting toolkit for ASTs.
//
// Features:
// - Generic tree terms with named symbols and string-valued atoms.
// - First-order pattern matching with variables ($name).
// - Rewrite rules, ordered rule sets, bottom-up/top-down strategies.
// - Fixed-point normalization with step limits and cycle detection.
// - Optional rewrite tracing through listeners.
// - No dependency on ParseTk.hpp; adapters can convert any AST to/from Term.
//
// Pattern syntax is represented by Term objects:
//   Term::var("x")                 variable
//   Term::node("Add", { ... })     constructor/application
//   Term::atom("42")               atom
//
// Variables match any term. A repeated variable must match the same term.
// Rule substitution is capture-free because this is first-order term rewriting.

#include <cstddef>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include <sstream>
#include <algorithm>
#include <initializer_list>

namespace rewrite {

class Term {
public:
    enum class Kind { Atom, Node, Variable };

    Term() = default;

    static Term atom(std::string value) {
        Term t;
        t.kind_ = Kind::Atom;
        t.value_ = std::move(value);
        return t;
    }

    static Term var(std::string name) {
        if (name.empty()) throw std::invalid_argument("empty rewrite variable");
        Term t;
        t.kind_ = Kind::Variable;
        t.value_ = std::move(name);
        return t;
    }

    static Term node(std::string symbol, std::vector<Term> children = {}) {
        if (symbol.empty()) throw std::invalid_argument("empty term symbol");
        Term t;
        t.kind_ = Kind::Node;
        t.value_ = std::move(symbol);
        t.children_ = std::move(children);
        return t;
    }

    static Term node(std::string symbol, std::initializer_list<Term> children) {
        return node(std::move(symbol), std::vector<Term>(children));
    }

    Kind kind() const noexcept { return kind_; }
    const std::string& value() const noexcept { return value_; }
    const std::vector<Term>& children() const noexcept { return children_; }
    std::vector<Term>& children() noexcept { return children_; }
    bool is_variable() const noexcept { return kind_ == Kind::Variable; }
    bool is_atom() const noexcept { return kind_ == Kind::Atom; }
    bool is_node() const noexcept { return kind_ == Kind::Node; }

    friend bool operator==(const Term& a, const Term& b) {
        return a.kind_ == b.kind_ && a.value_ == b.value_ &&
               a.children_ == b.children_;
    }
    friend bool operator!=(const Term& a, const Term& b) { return !(a == b); }

    std::string to_string() const {
        if (kind_ == Kind::Variable) return "$" + value_;
        if (kind_ == Kind::Atom) return quote_atom(value_);
        std::ostringstream out;
        out << quote_atom(value_);
        {
            out << '(';
            for (std::size_t i = 0; i < children_.size(); ++i) {
                if (i) out << ", ";
                out << children_[i].to_string();
            }
            out << ')';
        }
        return out.str();
    }

private:
    Kind kind_ = Kind::Atom;
    std::string value_;
    std::vector<Term> children_;

    static std::string quote_atom(std::string_view s) {
        bool simple = !s.empty();
        for (unsigned char c : s) {
            if (!(c >= 'a' && c <= 'z') && !(c >= 'A' && c <= 'Z') &&
                !(c >= '0' && c <= '9') && c != '_' && c != '.' && c != '-') {
                simple = false;
                break;
            }
        }
        if (simple) return std::string(s);
        std::string out = "\"";
        for (char c : s) {
            if (c == '\\' || c == '"') out.push_back('\\');
            if (c == '\n') { out += "\\n"; continue; }
            if (c == '\r') { out += "\\r"; continue; }
            if (c == '\t') { out += "\\t"; continue; }
            if (static_cast<unsigned char>(c) < 32 || static_cast<unsigned char>(c) == 127) {
                static constexpr char hex[] = "0123456789abcdef";
                out += "\\x"; out += hex[static_cast<unsigned char>(c)>>4]; out += hex[static_cast<unsigned char>(c)&15]; continue;
            }
            out.push_back(c);
        }
        out.push_back('"');
        return out;
    }
};

using Substitution = std::unordered_map<std::string, Term>;

namespace detail {
inline bool match_into(const Term& pattern, const Term& subject, Substitution& subst) {
    if (pattern.kind() == Term::Kind::Variable) {
        const auto it = subst.find(pattern.value());
        if (it == subst.end()) {
            subst.emplace(pattern.value(), subject);
            return true;
        }
        return it->second == subject;
    }

    if (pattern.kind() != subject.kind() ||
        pattern.value() != subject.value() ||
        pattern.children().size() != subject.children().size())
        return false;

    for (std::size_t i = 0; i < pattern.children().size(); ++i) {
        if (!match_into(pattern.children()[i], subject.children()[i], subst))
            return false;
    }
    return true;
}
inline void variables(const Term& t, std::unordered_set<std::string>& out) {
    if (t.is_variable()) out.insert(t.value());
    for (const auto& c : t.children()) variables(c, out);
}
inline void key(const Term& t, std::string& out) {
    out += std::to_string(static_cast<int>(t.kind())) + ":" + std::to_string(t.value().size()) + ":" + t.value();
    out += ":" + std::to_string(t.children().size()) + "[";
    for (const auto& c : t.children()) key(c, out);
    out += ']';
}
inline std::string key(const Term& t) { std::string out; key(t, out); return out; }
} // namespace detail

// Failure leaves the caller's substitution unchanged.
inline bool match(const Term& pattern, const Term& subject, Substitution& subst) {
    auto trial = subst;
    if (!detail::match_into(pattern, subject, trial)) return false;
    subst = std::move(trial);
    return true;
}

inline Term substitute(const Term& term, const Substitution& subst) {
    if (term.kind() == Term::Kind::Variable) {
        auto it = subst.find(term.value());
        if (it == subst.end())
            throw std::runtime_error("unbound variable in rewrite RHS: $" + term.value());
        return it->second;
    }

    if (term.children().empty()) {
        if (term.kind() == Term::Kind::Atom) return Term::atom(term.value());
        return Term::node(term.value());
    }

    std::vector<Term> children;
    children.reserve(term.children().size());
    for (const auto& child : term.children())
        children.push_back(substitute(child, subst));

    if (term.kind() == Term::Kind::Atom)
        return Term::atom(term.value());
    return Term::node(term.value(), std::move(children));
}

struct Rule {
    std::string name;
    Term lhs;
    Term rhs;

    Rule(std::string rule_name, Term left, Term right)
        : name(std::move(rule_name)), lhs(std::move(left)), rhs(std::move(right)) {
        if (lhs.kind() == Term::Kind::Variable)
            throw std::invalid_argument("a rewrite rule LHS cannot be a bare variable");
        std::unordered_set<std::string> left_vars, right_vars;
        detail::variables(lhs, left_vars); detail::variables(rhs, right_vars);
        for (const auto& v : right_vars)
            if (!left_vars.contains(v)) throw std::invalid_argument("unbound rewrite RHS variable: $" + v);
    }

    bool apply(const Term& input, Term& output) const {
        Substitution subst;
        if (!match(lhs, input, subst)) return false;
        output = substitute(rhs, subst);
        return true;
    }
};

enum class Direction { TopDown, BottomUp };
enum class EventKind { RuleApplied, TermChanged, NormalizationFinished, StepLimit, CycleDetected };

struct Event {
    EventKind kind = EventKind::RuleApplied;
    std::string rule_name;
    Term before;
    Term after;
    std::size_t step = 0;
    std::string message;
};

class Listener {
public:
    virtual ~Listener() = default;
    virtual void on_rewrite(const Event& event) = 0;
};

class EventStream {
public:
    using Callback = std::function<void(const Event&)>;

    void subscribe(Listener& listener) {
        callbacks_.emplace_back([&listener](const Event& e) { listener.on_rewrite(e); });
    }
    void subscribe(Callback callback) { callbacks_.push_back(std::move(callback)); }
    void emit(const Event& event) const {
        const auto snapshot = callbacks_;
        for (const auto& callback : snapshot) callback(event);
    }

private:
    std::vector<Callback> callbacks_;
};

struct Options {
    Direction direction = Direction::BottomUp;
    std::size_t max_steps = 10000;
    // Compatibility field: one-step normalization always revisits children
    // through subsequent traversals; replacement trees are never recursively
    // rewritten within a single budgeted step.
    bool normalize_children_after_rewrite = true;
    bool detect_cycles = true;
    std::size_t max_depth = 256;
    std::size_t max_nodes = 1'000'000;
};

struct Result {
    Term term;
    std::size_t steps = 0;
    bool normalized = false;
    bool cycle_detected = false;
    bool step_limit_reached = false;
    std::string error;
};

class System {
public:
    explicit System(Options options = {}) : options_(options) {}

    void add(Rule rule) { rules_.push_back(std::move(rule)); }
    const std::vector<Rule>& rules() const noexcept { return rules_; }
    Options& options() noexcept { return options_; }
    const Options& options() const noexcept { return options_; }
    EventStream& events() noexcept { return events_; }
    void subscribe(Listener& listener) { events_.subscribe(listener); }
    void subscribe(EventStream::Callback cb) { events_.subscribe(std::move(cb)); }

    // Apply the first matching rule at the current root.
    bool rewrite_root(Term& term, std::size_t step_number = 1) {
        for (const auto& rule : rules_) {
            Term next;
            if (rule.apply(term, next) && next != term) {
                Term before = term;
                term = std::move(next);
                events_.emit(Event{EventKind::RuleApplied, rule.name,
                                   std::move(before), term, step_number, {}});
                return true;
            }
        }
        return false;
    }

    Result normalize(Term input) {
        Result result;
        result.term = std::move(input);
        std::unordered_set<std::string> seen;

        if (!within_limits(result.term)) { result.error = "term depth or node limit exceeded"; return result; }
        if (options_.detect_cycles) seen.insert(detail::key(result.term));

        while (true) {
            // Probe without mutating at the budget boundary; an already-normal
            // term is still a normal form when max_steps == 0.
            if (result.steps == options_.max_steps) {
                if (!can_rewrite(result.term)) {
                    result.normalized = true;
                    return result;
                }
                break;
            }
            bool changed = false;
            if (options_.direction == Direction::BottomUp)
                changed = rewrite_bottom_up(result.term, result.steps);
            else
                changed = rewrite_top_down(result.term, result.steps);

            if (!changed) {
                result.normalized = true;
                events_.emit(Event{EventKind::NormalizationFinished, {},
                                   result.term, result.term, result.steps,
                                   "normal form reached"});
                return result;
            }

            ++result.steps;
            if (!within_limits(result.term)) { result.error = "term depth or node limit exceeded"; return result; }
            if (options_.detect_cycles) {
                const auto key = detail::key(result.term);
                if (!seen.insert(key).second) {
                    result.cycle_detected = true;
                    events_.emit(Event{EventKind::CycleDetected, {},
                                       result.term, result.term, result.steps,
                                       "rewrite cycle detected"});
                    return result;
                }
            }
        }

        result.step_limit_reached = true;
        events_.emit(Event{EventKind::StepLimit, {}, result.term, result.term,
                           result.steps, "maximum rewrite steps reached"});
        return result;
    }

private:
    Options options_;
    std::vector<Rule> rules_;
    EventStream events_;

    bool within_limits(const Term& term) const {
        std::vector<std::pair<const Term*, std::size_t>> todo{{&term, 0}};
        std::size_t count = 0;
        while (!todo.empty()) {
            auto [t, depth] = todo.back(); todo.pop_back();
            if (++count > options_.max_nodes || depth > options_.max_depth) return false;
            if (!t->is_node() && !t->children().empty()) return false;
            for (const auto& c : t->children()) todo.push_back({&c, depth + 1});
        }
        return true;
    }
    bool can_rewrite(const Term& term) const {
        for (const auto& rule : rules_) { Term next; if (rule.apply(term, next) && next != term) return true; }
        for (const auto& c : term.children()) if (can_rewrite(c)) return true;
        return false;
    }

    bool rewrite_top_down(Term& term, std::size_t step_number) {
        if (rewrite_root(term, step_number + 1)) return true;
        for (auto& child : term.children())
            if (rewrite_top_down(child, step_number)) return true;
        return false;
    }

    bool rewrite_bottom_up(Term& term, std::size_t step_number) {
        for (auto& child : term.children())
            if (rewrite_bottom_up(child, step_number)) return true;
        return rewrite_root(term, step_number + 1);
    }
};

inline Term var(std::string name) { return Term::var(std::move(name)); }
inline Term atom(std::string value) { return Term::atom(std::move(value)); }
inline Term node(std::string symbol, std::vector<Term> children = {}) {
    return Term::node(std::move(symbol), std::move(children));
}
inline Term node(std::string symbol, std::initializer_list<Term> children) {
    return Term::node(std::move(symbol), children);
}

} // namespace rewrite

#endif // REWRITE_HPP_INCLUDED
