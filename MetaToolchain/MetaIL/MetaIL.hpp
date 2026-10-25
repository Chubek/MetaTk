#ifndef METAIL_HPP
#define METAIL_HPP
#include "MetaIL.h"
#include <DSLtk.hpp>
#include <algorithm>
#include <charconv>
#include <concepts>
#include <cstdint>
#include <deque>
#include <fstream>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace metail {
class Error : public std::runtime_error {
public:
    explicit Error(const std::string &message) : std::runtime_error(message) {}
};

namespace detail {
inline void identifier(const std::string &name) {
    const auto first=[](char c) { return (c>='a' && c<='z') || (c>='A' && c<='Z') || c=='_'; };
    if (name.empty() || !first(name.front()) || std::any_of(name.begin()+1,name.end(),[&](char c) {
        return !first(c) && !(c>='0' && c<='9');
    })) throw Error("invalid IR identifier: "+name);
}
}

// DSLtk is the actual expression representation. Overloaded operators build
// trees; they never evaluate host arithmetic or assemble MetaIL source strings.
class Expr : public ::dsl::DSL<Expr, ::dsl::AST> {
    ::dsl::ASTNode tree_;
    static void validate(const ::dsl::ASTNode &n, unsigned depth, std::size_t &count) {
        if (depth>256 || ++count>200000) throw Error("expression exceeds IR limits");
        if (n.is_leaf() && n.size()) throw Error("expression leaf has children");
        if (n.tag().find('\0')!=std::string_view::npos) throw Error("embedded NUL in expression tag");
        if (n.tag()=="int" && n.is_leaf()) {
            std::int64_t value=0;
            const auto &s=n.value();auto result=std::from_chars(s.data(),s.data()+s.size(),value);
            if (result.ec!=std::errc{} || result.ptr!=s.data()+s.size()) throw Error("invalid signed-64 literal");
        } else if (n.tag()=="var" && n.is_leaf()) {
            detail::identifier(n.value());
            if (n.value()=="true" || n.value()=="false") throw Error("reserved variable name");
        } else if (n.tag()=="call" && !n.is_leaf()) {
            if (!n.size() || n.child(0).tag()!="callee" || !n.child(0).is_leaf() || n.child(0).size()) throw Error("call requires a callee leaf");
            const auto &name=n.child(0).value();detail::identifier(name);
            if (metail_operator_arity(name.c_str())>=0 || name=="true" || name=="false") throw Error("reserved call name");
            for (std::size_t i=1;i<n.size();++i) validate(n.child(i),depth+1,count);
        } else {
            const auto op=std::string(n.tag());int arity=metail_operator_arity(op.c_str());
            if (n.is_leaf() || arity<0 || n.size()!=static_cast<std::size_t>(arity)) throw Error("invalid expression operator/arity");
            for (const auto &child:n.children()) validate(child,depth+1,count);
        }
    }
    explicit Expr(::dsl::ASTNode tree) : tree_(std::move(tree)) {
        std::size_t count=0;validate(tree_,1,count);
    }
public:
    template<std::integral I> Expr(I value) {
        if constexpr (std::numeric_limits<I>::digits>63) {
            if (value>static_cast<I>(INT64_MAX)) throw Error("integer outside signed-64 range");
            if constexpr (std::numeric_limits<I>::is_signed)
                if (value<static_cast<I>(INT64_MIN)) throw Error("integer outside signed-64 range");
        }
        const auto text=std::to_string(static_cast<std::int64_t>(value));
        tree_=make_leaf<"int">(std::string_view(text));
    }
    static Expr variable(const std::string &name) { return Expr(make_leaf<"var">(std::string_view(name))); }
    static Expr call(const std::string &name, const std::vector<Expr> &args={}) {
        std::vector<::dsl::ASTNode> children{make_leaf<"callee">(std::string_view(name))};
        for (const auto &arg:args) children.push_back(arg.tree_);
        return Expr(::dsl::ASTNode("call",std::move(children)));
    }
    static Expr operation(const std::string &op, const std::vector<Expr> &args) {
        std::vector<::dsl::ASTNode> children;
        for (const auto &arg:args) children.push_back(arg.tree_);
        return Expr(::dsl::ASTNode(op,std::move(children)));
    }
    static Expr from_ast(::dsl::ASTNode tree) { return Expr(std::move(tree)); }
    const ::dsl::ASTNode &ast() const noexcept { return tree_; }
    bool same_as(const Expr &other) const { return tree_==other.tree_; }
    std::string dump() const { return tree_.dump(); } // DSLtk S-expression debug view.
    // DSLtk visitors receive const nodes, including the call's callee metadata.
    // References passed to callbacks are borrowed for the duration of this call.
    template<class Visitor>
    void visit(Visitor &visitor, ::dsl::VisitOrder order=::dsl::VisitOrder::Preorder) const {
        visitor.walk(tree_,order);
    }
    // Simultaneous syntactic substitution: inserted trees are not visited again.
    // This is not a semantics-preserving optimization (effects may be duplicated).
    Expr substitute(const std::map<std::string,Expr> &bindings) const {
        for (const auto &[name,value]:bindings) { (void)value;detail::identifier(name); }
        auto rules=::dsl::rewrite_set(::dsl::rule<"substitute-variable">(
            [&](const auto &n) { return n.tag()=="var" && bindings.contains(n.value()); },
            [&](const auto &n) { return bindings.at(n.value()).ast(); }));
        rules.max_iterations=1;
        return from_ast(rules.apply(tree_));
    }
#define METAIL_BINARY_OPERATOR(symbol, tag) \
    friend Expr operator symbol(const Expr &a, const Expr &b) { return Expr(make_node<tag>(a.tree_,b.tree_)); }
    METAIL_BINARY_OPERATOR(+, "add")
    METAIL_BINARY_OPERATOR(-, "sub")
    METAIL_BINARY_OPERATOR(*, "mul")
    METAIL_BINARY_OPERATOR(/, "div")
    METAIL_BINARY_OPERATOR(%, "mod")
    METAIL_BINARY_OPERATOR(&, "band")
    METAIL_BINARY_OPERATOR(|, "bor")
    METAIL_BINARY_OPERATOR(^, "bxor")
    METAIL_BINARY_OPERATOR(<<, "shl")
    METAIL_BINARY_OPERATOR(>>, "shr")
    METAIL_BINARY_OPERATOR(==, "eq")
    METAIL_BINARY_OPERATOR(!=, "ne")
    METAIL_BINARY_OPERATOR(<, "lt")
    METAIL_BINARY_OPERATOR(<=, "le")
    METAIL_BINARY_OPERATOR(>, "gt")
    METAIL_BINARY_OPERATOR(>=, "ge")
#undef METAIL_BINARY_OPERATOR
    friend Expr operator-(const Expr &a) { return Expr(make_node<"neg">(a.tree_)); }
    friend Expr operator~(const Expr &a) { return Expr(make_node<"bnot">(a.tree_)); }
    friend Expr operator!(const Expr &a) { return Expr(make_node<"lnot">(a.tree_)); }
};
inline Expr var(const std::string &name) { return Expr::variable(name); }
inline Expr integer(std::int64_t value) { return Expr(value); }
inline Expr call(const std::string &name, const std::vector<Expr> &args={}) { return Expr::call(name,args); }
inline Expr select(const Expr &condition, const Expr &yes, const Expr &no) {
    return Expr::from_ast(::dsl::node<"select">(condition.ast(),yes.ast(),no.ast()));
}
// Named logical builders avoid suggesting C++ short-circuit evaluation.
inline Expr land(const Expr &a, const Expr &b) { return Expr::operation("land",{a,b}); }
inline Expr lor(const Expr &a, const Expr &b) { return Expr::operation("lor",{a,b}); }

struct RewriteLimits {
    std::size_t max_passes=100; // Per expression; must be positive.
    std::size_t max_applications=10000;
};
struct RewriteResult {
    Expr expression;
    std::size_t passes; // Includes the final pass that proves convergence.
    std::size_t applications; // Structural changes, not identity matches.
};

// Checked, owning DSLtk rewrite result. Rules run bottom-up in declaration order.
// Every replacement and completed pass is validated. No partial result escapes
// on failure; callbacks' external side effects cannot be rolled back. Rules must
// preserve signed-64 arithmetic, evaluation order, effects and traps themselves.
// The explicit limits here supersede RewriteSet::max_iterations.
template<class... Rules>
RewriteResult rewrite(const Expr &input, const ::dsl::RewriteSet<Rules...> &rules,
                      RewriteLimits limits={}) {
    if (!limits.max_passes) throw Error("rewrite pass limit must be positive");
    std::size_t applications=0;
    auto checked=[&](const auto &rule) {
        return ::dsl::rule<"checked-metail-rule">(
            [](const auto &n) { return n.tag()!="callee"; },
            [&](const ::dsl::ASTNode &n) {
                auto replacement=rule.try_apply(n);
                if (!replacement) return n;
                try {
                    auto valid=Expr::from_ast(std::move(*replacement));
                    if (valid.ast()==n) return n;
                    if (applications==limits.max_applications) throw Error("rewrite application limit exceeded");
                    ++applications;
                    return valid.ast();
                } catch (const Error &error) {
                    throw Error("rewrite rule "+std::string(rule.name)+": "+error.what());
                }
            });
    };
    auto guarded=std::apply([&](const auto &...rule) { return ::dsl::rewrite_set(checked(rule)...); },rules.rules);
    guarded.max_iterations=1;
    Expr current=input;
    for (std::size_t pass=0;pass<limits.max_passes;++pass) {
        const auto before=applications;
        current=Expr::from_ast(guarded.apply(current.ast()));
        if (applications==before) return {std::move(current),pass+1,applications};
    }
    throw Error("rewrite pass limit exceeded before convergence");
}

struct Statement {
    metail_stmt_kind kind;
    std::string name;
    std::optional<Expr> expr;
    std::string target, yes, no;
};
struct Function {
    std::string name;
    std::vector<std::string> params;
    std::vector<Statement> statements;
    explicit Function(std::string name_, std::vector<std::string> params_={})
        : name(std::move(name_)),params(std::move(params_)) { detail::identifier(name); }
    Function &append(Statement statement) { statements.push_back(std::move(statement));return *this; }
    Function &let(std::string name_, Expr expr) {
        return append({METAIL_LET,std::move(name_),std::move(expr),{},{},{}});
    }
    Function &ret(Expr expr) { return append({METAIL_RETURN,{},std::move(expr),{},{},{}}); }
    Function &eval(Expr expr) { return append({METAIL_EVAL,{},std::move(expr),{},{},{}}); }
    Function &assume(Expr expr) { return append({METAIL_ASSUME,{},std::move(expr),{},{},{}}); }
    Function &label(std::string name_) { return append({METAIL_LABEL,std::move(name_),{},{},{},{}}); }
    Function &jump(std::string target) { return append({METAIL_GOTO,{},{},std::move(target),{},{}}); }
    Function &branch(Expr expr, std::string yes, std::string no) {
        return append({METAIL_BRANCH,{},std::move(expr),{},std::move(yes),std::move(no)});
    }
};
struct Module {
    std::optional<std::string> name;
    std::vector<Function> functions;
    explicit Module(std::string name_={}) {
        if (!name_.empty()) { detail::identifier(name_);name=std::move(name_); }
    }
    Module &add(Function function) { functions.push_back(std::move(function));return *this; }
    // Reference remains valid until functions is reallocated, as with std::vector.
    Function &function(std::string name_, std::vector<std::string> params={}) {
        functions.emplace_back(std::move(name_),std::move(params));return functions.back();
    }
};
struct ModuleOptimizationResult { Module module; std::string stats; };

struct ModuleRewriteResult {
    Module module;
    std::size_t expressions=0, passes=0, applications=0;
};
// Rewrites all expression-bearing statements in an independent module copy.
// The application budget is shared by the entire module. Names, control flow
// and bindings are unchanged; Context::check still validates the whole program.
template<class... Rules>
ModuleRewriteResult rewrite_expressions(const Module &input,
        const ::dsl::RewriteSet<Rules...> &rules, RewriteLimits limits={}) {
    if (!limits.max_passes) throw Error("rewrite pass limit must be positive");
    ModuleRewriteResult result{input};
    for (auto &function:result.module.functions) {
        for (auto &statement:function.statements) {
            if (!statement.expr) continue;
            auto rewritten=rewrite(*statement.expr,rules,
                {limits.max_passes,limits.max_applications-result.applications});
            statement.expr=std::move(rewritten.expression);
            ++result.expressions;
            result.passes+=rewritten.passes;
            result.applications+=rewritten.applications;
        }
    }
    return result;
}

namespace detail {
// Descriptors borrow strings from the value model for one protected C call.
class NativeModule {
    std::deque<metail_expr> exprs_;
    std::deque<std::vector<const metail_expr *>> args_;
    std::vector<std::vector<const char *>> params_;
    std::vector<std::vector<metail_stmt>> statements_;
    std::vector<metail_function> functions_;
    static const char *cstring(const std::string &s) {
        if (s.find('\0')!=std::string::npos) throw Error("embedded NUL in IR string");
        return s.c_str();
    }
    const metail_expr *lower(const ::dsl::ASTNode &n) {
        exprs_.push_back({});auto &e=exprs_.back();
        if (n.tag()=="int") {
            e.kind=METAIL_INT;const auto &s=n.value();
            std::from_chars(s.data(),s.data()+s.size(),e.value);
        } else if (n.tag()=="var") { e.kind=METAIL_VAR;e.name=n.value().c_str(); }
        else {
            const bool is_call=n.tag()=="call";
            e.kind=is_call ? METAIL_CALL : METAIL_OP;
            if (is_call) e.name=n.child(0).value().c_str();else e.op=n.tag().data();
            args_.emplace_back();auto &args=args_.back();
            for (std::size_t i=is_call ? 1 : 0;i<n.size();++i) args.push_back(lower(n.child(i)));
            e.args=args.data();e.arg_count=args.size();
        }
        return &e;
    }
public:
    metail_module value{};
    explicit NativeModule(const Module &module) {
        value.name=module.name ? cstring(*module.name) : nullptr;
        params_.resize(module.functions.size());statements_.resize(module.functions.size());
        functions_.reserve(module.functions.size());
        for (std::size_t i=0;i<module.functions.size();++i) {
            const auto &f=module.functions[i];
            for (const auto &p:f.params) params_[i].push_back(cstring(p));
            for (const auto &s:f.statements) {
                statements_[i].push_back({s.kind,cstring(s.name),s.expr ? lower(s.expr->ast()) : nullptr,
                    cstring(s.target),cstring(s.yes),cstring(s.no)});
            }
            functions_.push_back({cstring(f.name),params_[i].data(),params_[i].size(),
                statements_[i].data(),statements_[i].size()});
        }
        value.functions=functions_.data();value.function_count=functions_.size();
    }
    NativeModule(const NativeModule &)=delete;
    NativeModule &operator=(const NativeModule &)=delete;
};
inline Expr from_native(const metail_expr &e) {
    if (e.kind==METAIL_INT) return Expr(e.value);
    if (e.kind==METAIL_VAR) return var(e.name);
    std::vector<Expr> args;
    for (std::size_t i=0;i<e.arg_count;++i) args.push_back(from_native(*e.args[i]));
    return e.kind==METAIL_CALL ? call(e.name,args) : Expr::operation(e.op,args);
}
inline Module from_native(const metail_module &m) {
    Module result(m.name ? m.name : "");
    for (std::size_t i=0;i<m.function_count;++i) {
        const auto &f=m.functions[i];Function function(f.name);
        for (std::size_t j=0;j<f.param_count;++j) function.params.emplace_back(f.params[j]);
        for (std::size_t j=0;j<f.statement_count;++j) {
            const auto &s=f.statements[j];
            function.append({s.kind,s.name ? s.name : "",s.expr ? std::optional<Expr>(from_native(*s.expr)) : std::nullopt,
                s.target ? s.target : "",s.yes ? s.yes : "",s.no ? s.no : ""});
        }
        result.add(std::move(function));
    }
    return result;
}
} // namespace detail

struct OptimizationResult {
    std::string source;
    std::string stats; // JSON: {"kernel_changes": {name: count, ...}}
};

// Move-only RAII owner. All functions throw Error on runtime/argument failures.
// A context may be reused after an error; serialize calls to a single context.
class Context {
    std::unique_ptr<metail_context, decltype(&metail_destroy)> context_{nullptr,metail_destroy};
    using String = std::unique_ptr<char, decltype(&metail_free)>;
    static const char *cstring(const std::string &s) {
        if (s.find('\0')!=std::string::npos) throw Error("embedded NUL in MetaIL argument");
        return s.c_str();
    }
    metail_context *get() const {
        if (!context_) throw Error("moved-from MetaIL context");
        return context_.get();
    }
    void check_status(metail_status status) const {
        if (status!=METAIL_OK) throw Error(metail_last_error(get()));
    }
public:
    // Empty home selects automatic runtime discovery; explicit paths are strict.
    explicit Context(const std::string &home={}) {
        char error[2048];
        context_.reset(metail_create(home.empty() ? nullptr : cstring(home),error,sizeof error));
        if (!context_) throw Error(error);
    }
    Context(Context &&) noexcept = default;
    Context &operator=(Context &&) noexcept = default;
    Context(const Context &) = delete;
    Context &operator=(const Context &) = delete;
    metail_context *native_handle() const { return get(); } // Borrowed; do not destroy.

    Module parse(const std::string &source, const std::string &name="<memory>") const {
        metail_module *output=nullptr;
        auto status=metail_parse(get(),cstring(source),cstring(name),&output);
        std::unique_ptr<metail_module,decltype(&metail_module_free)> owned(output,metail_module_free);
        check_status(status);return detail::from_native(*owned);
    }
    void check(const Module &module) const {
        detail::NativeModule native(module);check_status(metail_module_check(get(),&native.value));
    }
    std::string emit(const Module &module) const {
        detail::NativeModule native(module);char *output=nullptr;
        auto status=metail_module_emit(get(),&native.value,&output);
        String owned(output,metail_free);check_status(status);return owned.get();
    }
    std::string analyze(const Module &module, const std::string &passes="all") const {
        detail::NativeModule native(module);char *output=nullptr;
        auto status=metail_module_analyze(get(),&native.value,cstring(passes),&output);
        String owned(output,metail_free);check_status(status);return owned.get();
    }
    ModuleOptimizationResult optimize(const Module &module, const std::string &passes="all", int rounds=2) const {
        detail::NativeModule native(module);metail_module *output=nullptr;char *stats=nullptr;
        auto status=metail_module_optimize(get(),&native.value,cstring(passes),rounds,&output,&stats);
        std::unique_ptr<metail_module,decltype(&metail_module_free)> owned(output,metail_module_free);
        String counts(stats,metail_free);check_status(status);return {detail::from_native(*owned),counts.get()};
    }

    void check(const std::string &source, const std::string &name="<memory>") const {
        check_status(metail_check(get(),cstring(source),cstring(name)));
    }
    std::string emit(const std::string &source, const std::string &name="<memory>") const {
        char *output=nullptr;
        auto status=metail_emit(get(),cstring(source),cstring(name),&output);
        String owned(output,metail_free);check_status(status);return owned.get();
    }
    OptimizationResult optimize(const std::string &source, const std::string &passes="all",
                                int rounds=2, const std::string &name="<memory>") const {
        char *output=nullptr,*stats=nullptr;
        auto status=metail_optimize(get(),cstring(source),cstring(name),cstring(passes),rounds,&output,&stats);
        String text(output,metail_free),counts(stats,metail_free);check_status(status);
        return {text.get(),counts.get()};
    }
    std::string analyze(const std::string &source, const std::string &passes="all",
                        const std::string &name="<memory>") const {
        char *output=nullptr;
        auto status=metail_analyze(get(),cstring(source),cstring(name),cstring(passes),&output);
        String owned(output,metail_free);check_status(status);return owned.get();
    }
    void load_kernels(const std::string &directory) const {
        check_status(metail_load_kernels(get(),cstring(directory)));
    }
    std::string kernels_json() const {
        char *output=nullptr;
        auto status=metail_kernels_json(get(),&output);
        String owned(output,metail_free);check_status(status);return owned.get();
    }
    static std::string read_file(const std::string &path) {
        std::ifstream file(cstring(path),std::ios::binary);
        if (!file) throw Error("cannot open "+path);
        std::ostringstream contents;contents<<file.rdbuf();
        if (file.bad() || contents.bad()) throw Error("cannot read "+path);
        return contents.str();
    }
};
} // namespace metail
#endif
