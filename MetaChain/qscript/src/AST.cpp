#include "qscript/QScript.h"
#include <string>

namespace qscript {
namespace {
struct Token { std::string text; std::size_t offset=0,line=1,column=1; };
class Parser {
    std::string_view source; Options options; std::string file;
    std::size_t pos=0,line=1,column=1,nodes=0; Token token;
    struct Failure { Diagnostic diagnostic; };
    [[noreturn]] void fail(std::string message) const {
        throw Failure{{metachain::Stage::Parse,std::move(message),file,token.line,token.column,token.offset}};
    }
    char take() { char c=source[pos++]; if (c=='\n') { ++line; column=1; } else ++column; return c; }
    void next() {
        for (;;) {
            while (pos<source.size()&&metachain::text::space(source[pos])) take();
            if (pos+1<source.size()&&source.substr(pos,2)=="//") { while (pos<source.size()&&take()!='\n') {} }
            else break;
        }
        token={"",pos,line,column}; if (pos==source.size()) return;
        auto start=pos; char c=take();
        if (metachain::text::alpha(c)) { while (pos<source.size()&&(metachain::text::alpha(source[pos])||metachain::text::digit(source[pos]))) take(); }
        else if (metachain::text::digit(c)) { while (pos<source.size()&&(metachain::text::alpha(source[pos])||metachain::text::digit(source[pos]))) take(); }
        else if (pos<source.size()) {
            auto pair=source.substr(start,2);
            if (pair=="=="||pair=="!="||pair=="<="||pair==">="||pair=="<<"||pair==">>") take();
        }
        token.text=std::string(source.substr(start,pos-start));
    }
    bool accept(std::string_view s) { if (token.text!=s) return false; next(); return true; }
    void expect(std::string_view s) { if (!accept(s)) fail("expected '"+std::string(s)+"'"); }
    void depth(std::size_t d) { if (d>options.max_depth) fail("syntax depth limit exceeded"); }
    dsl::ASTNode node(std::string tag,std::vector<dsl::ASTNode> children) {
        if (++nodes>options.max_nodes) fail("AST node limit exceeded");
        AST ast{dsl::ASTNode(std::move(tag),std::move(children))};
        // Bound left-associative trees as they grow, before a very long input
        // can create a recursively destructed tree deeper than the stack.
        if (!visit(ast,[](const auto&){return true;},options.max_depth)) fail("AST depth limit exceeded");
        return std::move(ast.root);
    }
    dsl::ASTNode leaf(std::string tag,std::string value) {
        if (++nodes>options.max_nodes) fail("AST node limit exceeded");
        return dsl::ASTNode(std::move(tag),std::move(value));
    }
    std::string identifier() {
        auto s=token.text;
        if (s.empty()||!metachain::text::alpha(s.front())||s=="let"||s=="if"||s=="else"||s=="while"||s=="return"||s=="print") fail("expected identifier");
        next(); return s;
    }
    static int precedence(std::string_view s) {
        if (s=="|") return 1;
        if (s=="^") return 2;
        if (s=="&") return 3;
        if (s=="=="||s=="!=") return 4;
        if (s=="<"||s==">"||s=="<="||s==">=") return 5;
        if (s=="<<"||s==">>") return 6;
        if (s=="+"||s=="-") return 7;
        if (s=="*"||s=="/"||s=="%") return 8;
        return 0;
    }
    dsl::ASTNode expression(std::size_t d,int minimum=1) {
        depth(d);
        auto lhs=[&]() -> dsl::ASTNode {
            if (accept("(")) { auto n=expression(d+1); expect(")"); return n; }
            if (token.text=="-"||token.text=="!"||token.text=="~") {
                auto op=token.text; next();
                // Permit INT64_MIN without first parsing an overflowing positive integer.
                if (op=="-"&&!token.text.empty()&&metachain::text::digit(token.text[0])) {
                    auto value=metachain::text::signed_integer("-"+token.text);
                    if (!value) fail("integer outside signed 64-bit range");
                    next(); return leaf("integer",std::to_string(*value));
                }
                return node(op=="-"?"neg":op=="!"?"not":"invert",{expression(d+1,9)});
            }
            if (!token.text.empty()&&metachain::text::digit(token.text[0])) {
                auto value=metachain::text::signed_integer(token.text); if (!value) fail("invalid signed 64-bit integer");
                next(); return leaf("integer",std::to_string(*value));
            }
            return leaf("name",identifier());
        }();
        while (precedence(token.text)>=minimum) {
            auto op=token.text; auto p=precedence(op); next(); auto rhs=expression(d+1,p+1);
            lhs=node(op,{std::move(lhs),std::move(rhs)});
        }
        return lhs;
    }
    dsl::ASTNode statement(std::size_t d) {
        depth(d);
        if (accept("{")) {
            std::vector<dsl::ASTNode> body;
            while (token.text!="}") { if (token.text.empty()) fail("unterminated block"); body.push_back(statement(d+1)); }
            next(); return node("block",std::move(body));
        }
        if (accept("let")) { auto name=leaf("name",identifier()); expect("="); auto value=expression(d+1); expect(";"); return node("let",{std::move(name),std::move(value)}); }
        if (token.text=="if"||token.text=="while") {
            auto kind=token.text; next(); expect("("); auto condition=expression(d+1); expect(")");
            auto body=statement(d+1); std::vector<dsl::ASTNode> children{std::move(condition),std::move(body)};
            if (kind=="if"&&accept("else")) children.push_back(statement(d+1));
            return node(kind,std::move(children));
        }
        if (token.text=="return"||token.text=="print") {
            auto kind=token.text; next(); auto value=expression(d+1); expect(";"); return node(kind,{std::move(value)});
        }
        auto name=leaf("name",identifier()); expect("="); auto value=expression(d+1); expect(";");
        return node("assign",{std::move(name),std::move(value)});
    }
public:
    Parser(std::string_view s,Options o,std::string f):source(s),options(std::move(o)),file(std::move(f)) {}
    Result<AST> run() {
        Result<AST> r;
        try {
            if (source.size()>options.toolchain.il.max_source_bytes) fail("source size limit exceeded");
            // Keep recursive routines bounded even if the caller requests an unsafe limit.
            options.max_depth=std::min<std::size_t>(options.max_depth,256);
            next(); std::vector<dsl::ASTNode> statements;
            while (!token.text.empty()) statements.push_back(statement(0));
            r.value=AST{node("program",std::move(statements))};
            if (!visit(*r,[](const auto&){return true;},options.max_depth)) { r.value.reset(); fail("AST depth limit exceeded"); }
        } catch (const Failure& e) { r.diagnostics.push_back(e.diagnostic); }
        return r;
    }
};
} // namespace
Result<AST> parse(std::string_view s,Options o,std::string file) { return Parser(s,std::move(o),std::move(file)).run(); }
} // namespace qscript
