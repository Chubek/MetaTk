#pragma once

// The collective MetaChain C++20 interface. The core is inline; optional
// backend algorithm entry points are supplied by MetaChain::Backend.
#include "metachain/Common.hpp"
#include "metachain/ParseTk.hpp"
#include "metachain/Rewrite.hpp"
#include "metachain/MetaVM.hpp"
#include "metachain/MetaIL.hpp"
#include "metachain/MetaJIT.hpp"
#include "metachain/LDtk.hpp"
#include "metachain/ASMtk.hpp"
#include "metachain/Native.hpp"
#include "metachain/Text.hpp"
#include "metachain/ISel.hpp"
#include "metachain/ISched.hpp"
#include "metachain/Regalloc.hpp"

// Library version, also available as macros for conditional compilation.
#define METACHAIN_VERSION_MAJOR 1
#define METACHAIN_VERSION_MINOR 0
#define METACHAIN_VERSION_PATCH 0

namespace metachain {
namespace parsing = ::parsetk;
namespace rewriting = ::rewrite;
namespace il = ::metail;
namespace vm = ::metavm;
namespace jit = ::metajit;
namespace assembly = ::asmtk;
namespace linking = ::ld_tk;
inline constexpr std::string_view version = "1.0.0";
enum class Stage { Parse, Verify, Optimize, Execute, Assemble, Link, Lower, Rewrite, IO };
struct Diagnostic {
    Stage stage = Stage::Parse;
    std::string message, file;
    std::size_t line = 0, column = 0, offset = 0;
};
template<class T> struct Outcome {
    std::optional<T> value;
    std::vector<Diagnostic> diagnostics;
    explicit operator bool() const noexcept { return value.has_value() && diagnostics.empty(); }
    T& operator*() { return value.value(); }
    const T& operator*() const { return value.value(); }
    T* operator->() { return &value.value(); }
    const T* operator->() const { return &value.value(); }
};
// Lossless DSLtk AST <-> first-order term bridge. Tags and leaf values are
// separate atoms; a leaf is never confused with a zero-child inner node.
// Recursion is depth bounded; an oversized C++ value graph throws rather than
// exhausting the C++ stack. The inverse reports a diagnostic instead.
inline rewrite::Term ast_to_term(const dsl::ASTNode& ast, std::size_t max_depth = 256) {
    std::function<rewrite::Term(const dsl::ASTNode&, std::size_t)> convert =
        [&](const dsl::ASTNode& node, std::size_t depth) -> rewrite::Term {
        if (depth > max_depth)
            throw std::invalid_argument("AST exceeds the term-encoding depth limit");
        if (node.is_leaf())
            return rewrite::node("leaf",{rewrite::atom(std::string(node.tag())),rewrite::atom(node.value())});
        std::vector<rewrite::Term> children{rewrite::atom(std::string(node.tag()))};
        children.reserve(node.size()+1);
        for (const auto& c:node.children()) children.push_back(convert(c,depth+1));
        return rewrite::node("node",std::move(children));
    };
    return convert(ast,0);
}
inline Outcome<dsl::ASTNode> term_to_ast(const rewrite::Term& term, std::size_t max_depth = 256) {
    Outcome<dsl::ASTNode> out;
    std::function<std::optional<dsl::ASTNode>(const rewrite::Term&,std::size_t)> convert = [&](const auto& t,std::size_t depth) -> std::optional<dsl::ASTNode> {
        if (depth>max_depth || !t.is_node() || t.children().empty() || !t.children()[0].is_atom()) return std::nullopt;
        const auto& tag=t.children()[0].value();
        if (t.value()=="leaf" && t.children().size()==2 && t.children()[1].is_atom()) return dsl::ASTNode(tag,t.children()[1].value());
        if (t.value()!="node") return std::nullopt;
        std::vector<dsl::ASTNode> children;
        for (std::size_t n=1;n<t.children().size();++n) {
            auto c=convert(t.children()[n],depth+1); if (!c) return std::nullopt; children.push_back(std::move(*c));
        }
        return dsl::ASTNode(tag,std::move(children));
    };
    out.value=convert(term,0);
    if (!out.value) out.diagnostics.push_back({Stage::Rewrite,"invalid AST term encoding or AST depth limit exceeded",{},0,0,0});
    return out;
}
struct Options {
    metail::Limits il{};
    metavm::VMOptions vm{};
    asmtk::Options assembler{};
    ld_tk::Options linker{};
    rewrite::Options rewriting{};
    parsetk::Options parser{};
    text::ParseLimits text{};
    bool optimize = true;
    metajit::Options jit{};
};
class Toolchain : public dsl::DSL<Toolchain, dsl::Pipeline, dsl::AST> {
    Options options_;
    template<class T> static void il_errors(Outcome<T>& out, const std::vector<metail::Diagnostic>& errors, Stage stage) {
        for (const auto& e : errors) out.diagnostics.push_back({stage,e.message,e.source.file,e.source.line,e.source.column,e.instruction});
    }
public:
    explicit Toolchain(Options o = {}) : options_(std::move(o)) {}
    const Options& options() const noexcept { return options_; }
    Outcome<std::vector<parsetk::Event>> parse(std::string_view input, parsetk::Registry grammar, std::string_view start) const {
        Outcome<std::vector<parsetk::Event>> out; std::vector<parsetk::Event> events;
        parsetk::Parser parser(std::move(grammar),options_.parser);
        parser.subscribe([&](const auto& e) { events.push_back(e); });
        auto r=parser.parse(input,start);
        if (!r) out.diagnostics.push_back({Stage::Parse,r.error,{},r.error_span.line,r.error_span.column,r.error_span.offset});
        else out.value=std::move(events);
        return out;
    }
    Outcome<dsl::ASTNode> parse_ast(std::string_view input, parsetk::Registry grammar, std::string_view start) const {
        auto events=parse(input,std::move(grammar),start);
        if (!events) return {std::nullopt,std::move(events.diagnostics)};
        std::vector<dsl::ASTNode> stack;
        for (const auto& e:*events) {
            if (e.kind==parsetk::EventKind::RuleBegin) stack.emplace_back(e.name,std::vector<dsl::ASTNode>{});
            else if (e.kind==parsetk::EventKind::Token) stack.back().children().emplace_back(e.name,e.text);
            else if (e.kind==parsetk::EventKind::RuleEnd && stack.size()>1) {
                auto node=std::move(stack.back()); stack.pop_back(); stack.back().children().push_back(std::move(node));
            }
        }
        return {std::move(stack.front()),{}};
    }
    Outcome<rewrite::Term> rewrite(std::string_view source, std::string_view rules) const {
        auto subject = parse_term(source);
        auto system = parse_rules(rules);
        Outcome<rewrite::Term> out;
        if (!subject) out.diagnostics = std::move(subject.diagnostics);
        if (!system) out.diagnostics.insert(out.diagnostics.end(),system.diagnostics.begin(),system.diagnostics.end());
        if (!out.diagnostics.empty()) return out;
        return normalize(std::move(*subject.value),*system.value);
    }
    Outcome<metail::Program> compile(std::string_view source, std::string file = {}) const {
        Outcome<metail::Program> out;
        auto r = metail::parse(source,std::move(file),options_.il);
        il_errors(out,r.diagnostics,Stage::Parse);
        if (out.diagnostics.empty()) {
            if (options_.optimize) { auto opt = metail::optimize(std::move(r.program)); il_errors(out,opt.diagnostics,Stage::Optimize); out.value=std::move(opt.program); }
            else out.value=std::move(r.program);
        }
        return out;
    }
    Outcome<metail::Program> decode(std::span<const std::uint8_t> bytes) const {
        Outcome<metail::Program> out; auto r=metail::decode(bytes,options_.il);
        il_errors(out,r.diagnostics,Stage::Verify); if (out.diagnostics.empty()) out.value=std::move(r.program); return out;
    }
    Outcome<metail::Execution> run(const metail::Program& p) const {
        Outcome<metail::Execution> out;
        auto errors=metail::verify(p,options_.il); il_errors(out,errors,Stage::Verify);
        if (!out.diagnostics.empty()) return out;
        auto r=metail::run(p,options_.vm,options_.il); il_errors(out,r.diagnostics,Stage::Execute);
        out.value=std::move(r); return out;
    }
    Outcome<metail::Execution> run(std::string_view source) const {
        auto p=compile(source); if (!p) return {std::nullopt,std::move(p.diagnostics)};
        return run(*p);
    }
    Outcome<metajit::Execution> jit(const metail::Program& p) const {
        Outcome<metajit::Execution> out;
        il_errors(out,metail::verify(p,options_.il),Stage::Verify);
        if (!out.diagnostics.empty()) return out;
        auto r=metajit::run(p,options_.jit,options_.vm,options_.il);
        il_errors(out,r.diagnostics,Stage::Execute); out.value=std::move(r); return out;
    }
    Outcome<metajit::Execution> jit(std::string_view source) const {
        auto p=compile(source); if (!p) return {std::nullopt,std::move(p.diagnostics)};
        return jit(*p);
    }
    Outcome<std::vector<std::uint8_t>> executable(std::string_view source) const {
        auto p=compile(source); if (!p) return {std::nullopt,std::move(p.diagnostics)};
        return executable(*p);
    }
    Outcome<std::vector<std::uint8_t>> assemble(std::string_view source, bool object = false) const {
        Outcome<std::vector<std::uint8_t>> out; asmtk::Assembler a(options_.assembler);
        auto r=object ? a.assemble_object(source) : a.assemble(source);
        for (const auto& e:r.diagnostics) out.diagnostics.push_back({Stage::Assemble,e.message,{},e.location.line,e.location.column,e.location.offset});
        if (out.diagnostics.empty()) out.value=std::move(r.bytes);
        return out;
    }
    Outcome<std::vector<std::uint8_t>> link(const std::vector<std::vector<std::uint8_t>>& objects, const std::vector<std::string>& names = {}) const {
        Outcome<std::vector<std::uint8_t>> out; auto r=ld_tk::Linker(options_.linker).link(objects,names);
        for (const auto& e:r.diagnostics) out.diagnostics.push_back({Stage::Link,e.message,e.object,0,0,e.offset});
        if (out.diagnostics.empty()) out.value=std::move(r.image);
        return out;
    }
    // Filesystem access is an explicit, separately staged operation. Nothing
    // else in the facade touches the filesystem, and no output path is written.
    Outcome<std::vector<std::uint8_t>> link_files(const std::vector<std::string>& paths) const {
        Outcome<std::vector<std::uint8_t>> out;
        if (paths.empty()) { out.diagnostics.push_back({Stage::IO,"no input object paths supplied",{},0,0,0}); return out; }
        if (paths.size() > options_.linker.max_objects) { out.diagnostics.push_back({Stage::IO,"input object count limit exceeded",{},0,0,0}); return out; }
        auto r = ld_tk::Linker(options_.linker).link_files(paths);
        if (r.image.empty()) {
            // Distinguish I/O from format/link diagnostics for callers.
            const bool io = !r.diagnostics.empty() && r.diagnostics.front().message.rfind("cannot read input file",0) == 0;
            for (const auto& e:r.diagnostics)
                out.diagnostics.push_back({io ? Stage::IO : Stage::Link,e.message,e.object,0,0,e.offset});
            return out;
        }
        for (const auto& e:r.diagnostics) out.diagnostics.push_back({Stage::Link,e.message,e.object,0,0,e.offset});
        if (out.diagnostics.empty()) out.value = std::move(r.image);
        return out;
    }
    Outcome<rewrite::Term> parse_term(std::string_view source) const {
        Outcome<rewrite::Term> out; auto r=text::parse_term(source,options_.text);
        if (!r) out.diagnostics.push_back({Stage::Parse,r.error,{},0,0,r.offset});
        else out.value=std::move(*r.term);
        return out;
    }
    Outcome<std::vector<rewrite::Rule>> parse_rules(std::string_view source) const {
        Outcome<std::vector<rewrite::Rule>> out; auto r=text::parse_rules(source,options_.text);
        if (!r) out.diagnostics.push_back({Stage::Parse,r.error,{},r.line,0,r.offset});
        else out.value=std::move(r.rules);
        return out;
    }
    Outcome<std::string> native_source(const metail::Program& p) const {
        Outcome<std::string> out;
        il_errors(out,metail::verify(p,options_.il),Stage::Verify);
        if (!out.diagnostics.empty()) return out;
        auto r=lower_linux_x86_64(p,options_.vm,options_.assembler.max_source_bytes);
        il_errors(out,r.diagnostics,Stage::Lower); if (out.diagnostics.empty()) out.value=std::move(r.assembly); return out;
    }
    Outcome<std::vector<std::uint8_t>> executable(const metail::Program& p) const {
        auto source=native_source(p); if (!source) return {std::nullopt,std::move(source.diagnostics)};
        auto object=assemble(*source,true); if (!object) return object;
        return link({*object});
    }
    Outcome<rewrite::Term> normalize(rewrite::Term term, const std::vector<rewrite::Rule>& rules) const {
        rewrite::System system(options_.rewriting); for (const auto& rule:rules) system.add(rule);
        auto r=system.normalize(std::move(term)); Outcome<rewrite::Term> out;
        if (!r.normalized) out.diagnostics.push_back({Stage::Rewrite,!r.error.empty()?r.error:r.cycle_detected?"rewrite cycle detected":"rewrite step limit exceeded",{},0,0,r.steps});
        out.value=std::move(r.term); return out;
    }
    // DSLtk stages own a copy of the configuration; no dangling `this` capture.
    auto compile_stage() const { return dsl::pipe([tc=*this](std::string_view s) { return tc.compile(s); }); }
    auto run_stage() const {
        return dsl::pipe([tc=*this](Outcome<metail::Program> p) -> Outcome<metail::Execution> {
            if (!p) return {std::nullopt,std::move(p.diagnostics)};
            return tc.run(*p);
        });
    }
    auto native_stage() const {
        return dsl::pipe([tc=*this](Outcome<metail::Program> p) -> Outcome<std::string> {
            if (!p) return {std::nullopt,std::move(p.diagnostics)};
            return tc.native_source(*p);
        });
    }
    auto executable_stage() const {
        return dsl::pipe([tc=*this](Outcome<metail::Program> p) -> Outcome<std::vector<std::uint8_t>> {
            if (!p) return {std::nullopt,std::move(p.diagnostics)};
            return tc.executable(*p);
        });
    }
    // Text-level convenience: MCIL in, static ELF executable bytes out.
    auto build_stage() const {
        return dsl::pipe([tc=*this](std::string_view source) { return tc.executable(source); });
    }
};

namespace native {
// Expression trees are DSLtk AST values, not a second expression framework.
struct Expr {
    dsl::ASTNode tree;
    Expr(std::int64_t v) : tree(dsl::leaf<"integer">(v)) {}
    explicit Expr(dsl::ASTNode t) : tree(std::move(t)) {}
};
struct Register {
    std::uint8_t index;
    explicit Register(unsigned n) : index(static_cast<std::uint8_t>(n)) {
        if (n >= 16) throw std::invalid_argument("native DSL user registers are r0..r15");
    }
    operator Expr() const { return Expr(dsl::leaf<"register">(unsigned(index))); }
};
inline Register reg(unsigned n) { return Register(n); }
inline Expr operator+(Expr a, Expr b) { return Expr(dsl::node<"add">(a.tree,b.tree)); }
inline Expr operator-(Expr a, Expr b) { return Expr(dsl::node<"sub">(a.tree,b.tree)); }
inline Expr operator*(Expr a, Expr b) { return Expr(dsl::node<"mul">(a.tree,b.tree)); }
inline Expr operator/(Expr a, Expr b) { return Expr(dsl::node<"div">(a.tree,b.tree)); }
inline Expr operator%(Expr a, Expr b) { return Expr(dsl::node<"mod">(a.tree,b.tree)); }
inline Expr equal(Expr a, Expr b) { return Expr(dsl::node<"eq">(a.tree,b.tree)); }
inline Expr less(Expr a, Expr b) { return Expr(dsl::node<"lt">(a.tree,b.tree)); }
class Script : public dsl::DSL<Script, dsl::Pipeline, dsl::AST> {
    metail::Builder builder_;
    std::vector<Diagnostic> errors_;
    unsigned temporary_ = 16;
    std::uint8_t expression(const dsl::ASTNode& node, std::size_t depth = 0) {
        if (depth > 256) throw std::invalid_argument("expression exceeds the AST depth limit");
        if (temporary_ >= 32) throw std::invalid_argument("expression requires more than 16 temporary registers");
        if (node.tag()=="integer" && node.is_leaf()) {
            auto v=text::signed_integer(node.value()); if (!v) throw std::invalid_argument("invalid DSL integer");
            auto r=static_cast<std::uint8_t>(temporary_++); builder_.constant(r,*v); return r;
        }
        if (node.tag()=="register" && node.is_leaf()) {
            auto v=text::integer(node.value()); if (!v || v->negative || v->magnitude>=16) throw std::invalid_argument("invalid DSL register");
            auto r=static_cast<std::uint8_t>(temporary_++); builder_.emit({metail::Op::Mov,r,static_cast<std::uint8_t>(v->magnitude),0,0}); return r;
        }
        auto op=metail::opcode(node.tag());
        if (!op || !metail::binary(*op) || node.is_leaf() || node.size()!=2) throw std::invalid_argument("invalid DSL expression tree");
        const auto a=expression(node.child(0),depth+1), b=expression(node.child(1),depth+1);
        builder_.binary(*op,a,a,b); --temporary_; return a;
    }
public:
    Script& set(Register dest, Expr value) {
        // Transactional expression lowering: failed expressions emit no code.
        auto before=builder_; temporary_=16;
        try { auto src=expression(value.tree); builder_.emit({metail::Op::Mov,dest.index,src,0,0}); }
        catch (const std::invalid_argument& e) { builder_=std::move(before); errors_.push_back({Stage::Lower,e.what(),{},0,0,0}); }
        return *this;
    }
    Script& label(std::string n) { builder_.label(std::move(n)); return *this; }
    Script& jump(std::string n) { builder_.jump(metail::Op::Jmp,std::move(n)); return *this; }
    Script& while_nonzero(Register r, std::string target) { builder_.jump(metail::Op::Jnz,std::move(target),r.index); return *this; }
    Script& jump_zero(Register r, std::string target) { builder_.jump(metail::Op::Jz,std::move(target),r.index); return *this; }
    Script& print(Register r) { builder_.emit({metail::Op::Print,r.index,0,0,0}); return *this; }
    Script& halt(Register r = Register(0)) { builder_.halt(r.index); return *this; }
    Script& emit(metail::Instruction i) { builder_.emit(i); return *this; }
    Outcome<metail::Program> finish() const {
        auto r=builder_.finish(); Outcome<metail::Program> out; out.diagnostics=errors_;
        for (const auto& e:r.diagnostics) out.diagnostics.push_back({Stage::Verify,e.message,{},e.source.line,e.source.column,e.instruction});
        if (out.diagnostics.empty()) out.value=std::move(r.program);
        return out;
    }
};
}
using Script = native::Script;

// A textual command dispatcher for embedding or the optional command-line
// driver. Commands and payloads are separate, so no shell evaluation occurs.
struct TextResult {
    std::string text;
    std::vector<std::uint8_t> bytes;
    std::vector<Diagnostic> diagnostics;
    explicit operator bool() const noexcept { return diagnostics.empty(); }
};
inline TextResult evaluate(std::string_view command, std::string_view source, Options options = {}) {
    Toolchain tc(options); TextResult out;
    if (command=="parse" || command=="rewrite") {
        // A line containing only %% separates grammar/rules from input.
        std::size_t pos=0, separator=source.npos, payload=0;
        while (pos<=source.size()) {
            auto end=source.find('\n',pos); if (end==source.npos) end=source.size();
            if (text::trim(source.substr(pos,end-pos))=="%%") { separator=pos; payload=end<source.size()?end+1:end; break; }
            if (end==source.size()) break;
            pos=end+1;
        }
        if (separator==source.npos) { out.diagnostics.push_back({Stage::Parse,"expected a %% separator line",{},0,0,0}); return out; }
        if (command=="parse") {
            auto grammar=text::parse_grammar(source.substr(0,separator),options.parser);
            if (!grammar) { out.diagnostics.push_back({Stage::Parse,grammar.error,{},grammar.line,0,0}); return out; }
            auto r=tc.parse(source.substr(payload),std::move(grammar.registry),grammar.start);
            out.diagnostics=std::move(r.diagnostics);
            if (r.value) for (const auto& e:*r) if (e.kind==parsetk::EventKind::Token) {
                std::ostringstream line; line << e.name << ' ' << std::quoted(e.text) << '\n'; out.text+=line.str();
            }
        } else {
            auto r=tc.rewrite(source.substr(payload),source.substr(0,separator));
            out.diagnostics=std::move(r.diagnostics); if (r.value) out.text=r->to_string()+"\n";
        }
    } else if (command=="run") {
        auto r=tc.run(source); out.diagnostics=std::move(r.diagnostics);
        if (r.value) { for (auto n:r->output) out.text+=std::to_string(n)+"\n"; out.text+="result "+std::to_string(r->value)+"\n"; }
    } else if (command=="check" || command=="optimize" || command=="encode" || command=="native" || command=="build") {
        auto p=tc.compile(source); out.diagnostics=std::move(p.diagnostics);
        if (!p.value || !out.diagnostics.empty()) return out;
        if (command=="check") out.text="ok\n";
        else if (command=="optimize") { auto r=metail::optimize(*p); for (const auto& e:r.diagnostics) out.diagnostics.push_back({Stage::Optimize,e.message,{},0,0,e.instruction}); out.text=metail::emit(r.program); }
        else if (command=="encode") out.bytes=metail::encode(*p);
        else if (command=="native") { auto r=tc.native_source(*p); out.diagnostics=std::move(r.diagnostics); if (r.value) out.text=std::move(*r); }
        else { auto r=tc.executable(*p); out.diagnostics=std::move(r.diagnostics); if (r.value) out.bytes=std::move(*r); }
    } else if (command=="assemble" || command=="object" || command=="link-assembly") {
        auto r=tc.assemble(source,command!="assemble");
        if (r && command=="link-assembly") r=tc.link({*r});
        out.diagnostics=std::move(r.diagnostics); if (r.value) out.bytes=std::move(*r);
    } else out.diagnostics.push_back({Stage::Parse,"unknown command: "+std::string(command),{},0,0,0});
    return out;
}
inline std::string format(const Diagnostic& d) {
    std::ostringstream out; if (!d.file.empty()) out << d.file << ':';
    if (d.line) out << d.line << ':' << d.column << ": ";
    out << d.message; return out.str();
}
} // namespace metachain
