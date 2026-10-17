#pragma once
#include "Common.hpp"
#include "ParseTk.hpp"
#include "Rewrite.hpp"
#include <set>

namespace metachain::text {
struct ParseLimits {
    std::size_t max_bytes = 64u * 1024u * 1024u;
    std::size_t max_nodes = 1'000'000;
    std::size_t max_depth = 256;
};
namespace detail {
class TermReader {
    dsl::ParsecInput& in_;
    ParseLimits limits_;
    std::size_t nodes_ = 0;
    void ws() { while (!in_.eof() && space(in_.peek())) in_.consume(); }
    dsl::ExpectedResult<rewrite::Term> fail(std::string message) {
        return dsl::ExpectedResult<rewrite::Term>::failure(in_.pos,dsl::ParseFailureKind::Committed,{std::move(message)});
    }
    dsl::ExpectedResult<rewrite::Term> node(std::string symbol, std::size_t depth) {
        in_.consume(); ws(); std::vector<rewrite::Term> children;
        if (in_.peek()!=')') while (true) {
            auto child=term(depth+1); if (!child) return child;
            children.push_back(std::move(*child)); ws();
            if (in_.peek()!=',') break;
            in_.consume();
        }
        if (in_.consume()!=')') return fail("expected ')' after node children");
        if (symbol.empty()) return fail("empty node symbol");
        return rewrite::Term::node(std::move(symbol),std::move(children));
    }
    dsl::ExpectedResult<rewrite::Term> term(std::size_t depth) {
        ws();
        if (depth > limits_.max_depth || ++nodes_ > limits_.max_nodes) return fail("term depth or node limit exceeded");
        if (in_.eof()) return fail("expected term");
        const auto start=in_.pos;
        if (in_.peek()=='\'' || in_.peek()=='"') {
            const char quote=in_.consume(); bool escape=false,closed=false;
            while (!in_.eof()) {
                const char c=in_.consume();
                if (escape) escape=false;
                else if (c=='\\') escape=true;
                else if (c==quote) { closed=true; break; }
            }
            if (!closed) return fail("unterminated quoted atom");
            auto value=unquote(in_.get_span(start));
            if (!value) return fail("invalid quoted atom escape");
            ws();
            if (in_.peek()=='(') return node(std::move(*value),depth);
            return rewrite::Term::atom(std::move(*value));
        }
        while (!in_.eof() && !space(in_.peek()) && in_.peek()!=',' && in_.peek()!='(' && in_.peek()!=')' && in_.peek()!='=') in_.consume();
        auto token=in_.get_span(start);
        if (token.empty()) return fail("expected atom, variable, or node");
        ws();
        if (token[0]=='$') {
            if (!identifier(token.substr(1))) return fail("invalid rewrite variable");
            return rewrite::Term::var(std::string(token.substr(1)));
        }
        if (in_.peek()!='(') return rewrite::Term::atom(std::string(token));
        return node(std::string(token),depth);
    }
public:
    TermReader(dsl::ParsecInput& in, ParseLimits limits) : in_(in), limits_(limits) {}
    dsl::ExpectedResult<rewrite::Term> read() {
        if (in_.source.size()>limits_.max_bytes) return fail("term source size limit exceeded");
        return term(0);
    }
};
}
inline auto term_parser(ParseLimits limits = {}) {
    return dsl::parser([limits](dsl::ParsecInput& in) -> dsl::ExpectedResult<rewrite::Term> { return detail::TermReader(in,limits).read(); });
}
struct TermResult {
    std::optional<rewrite::Term> term;
    std::string error;
    std::size_t offset=0;
    explicit operator bool() const { return term.has_value() && error.empty(); }
};
inline TermResult parse_term(std::string_view source, ParseLimits limits = {}) {
    dsl::ParsecInput in{source,0}; auto result=term_parser(limits)(in);
    if (!result) return {std::nullopt,result.error.expected.empty()?"invalid term":result.error.expected[0],result.error.pos};
    while (!in.eof() && space(in.peek())) in.consume();
    if (!in.eof()) return {std::nullopt,"unexpected trailing term input",in.pos};
    return {std::move(*result),{},in.pos};
}
struct RuleResult {
    std::vector<rewrite::Rule> rules;
    std::string error;
    std::size_t line=0, offset=0;
    explicit operator bool() const { return error.empty(); }
};
inline RuleResult parse_rules(std::string_view source, ParseLimits limits = {}) {
    RuleResult r;
    if (source.size()>limits.max_bytes) { r.error="rule source size limit exceeded"; return r; }
    std::size_t pos=0,line_no=1;
    while (pos<=source.size()) {
        auto end=source.find('\n',pos); if (end==source.npos) end=source.size();
        auto line=trim(uncomment(source.substr(pos,end-pos)));
        if (!line.empty()) {
            const auto colon=line.find(':');
            auto fail=[&](std::string e, std::size_t p=0) { r.error=std::move(e); r.line=line_no; r.offset=pos+p; };
            if (colon==line.npos || !identifier(trim(line.substr(0,colon)))) { fail("expected rule-name: lhs => rhs"); return r; }
            dsl::ParsecInput in{line,colon+1}; const auto parser=term_parser(limits);
            auto lhs=parser(in);
            if (!lhs) { fail("invalid rule left-hand term",in.pos); return r; }
            while (!in.eof() && space(in.peek())) in.consume();
            if (line.substr(in.pos,2)!="=>") { fail("expected '=>' between rule terms",in.pos); return r; }
            in.pos+=2; auto rhs=parser(in);
            if (!rhs) { fail("invalid rule right-hand term",in.pos); return r; }
            while (!in.eof() && space(in.peek())) in.consume();
            if (!in.eof()) { fail("unexpected trailing rule input",in.pos); return r; }
            try { r.rules.emplace_back(std::string(trim(line.substr(0,colon))),std::move(*lhs),std::move(*rhs)); }
            catch (const std::invalid_argument& e) { fail(e.what()); return r; }
        }
        if (end==source.size()) break;
        pos=end+1; ++line_no;
    }
    return r;
}
struct GrammarResult {
    parsetk::Registry registry;
    std::string start, error;
    std::size_t line=0;
    explicit operator bool() const { return error.empty(); }
};
// One declaration per line: lexeme NAME = "regex", keyword NAME = "text",
// rule NAME = PEG expression. The first rule is the default start production.
inline GrammarResult parse_grammar(std::string_view source, parsetk::Options options = {}) {
    GrammarResult r;
    if (source.size()>options.max_grammar_bytes) { r.error="grammar source size limit exceeded"; return r; }
    std::set<std::string> names;
    std::size_t pos=0,line_no=1;
    while (pos<=source.size()) {
        auto end=source.find('\n',pos); if (end==source.npos) end=source.size();
        auto line=trim(source.substr(pos,end-pos));
        if (!line.empty() && !line.starts_with("//")) {
            const auto split=line.find_first_of(" \t"), equals=line.find('=');
            auto fail=[&](std::string e) { r.error=std::move(e); r.line=line_no; };
            if (split==line.npos || equals==line.npos || equals<=split) { fail("expected kind NAME = body"); return r; }
            auto kind=line.substr(0,split), n=trim(line.substr(split,equals-split)), body=trim(line.substr(equals+1));
            if (n.empty() || (!identifier(n) && !((n[0]=='@' || n[0]=='#') && identifier(n.substr(1)))) || !names.insert(std::string(n)).second) {
                fail("invalid or duplicate grammar symbol"); return r;
            }
            if (kind=="rule") {
                if (r.start.empty()) r.start=std::string(n);
                r.registry.add_symbol({parsetk::Symbol::Kind::Production,std::string(n),{},std::string(body)});
            } else if (kind=="lexeme" || kind=="keyword") {
                auto value=unquote(body);
                if (!value) { fail("expected quoted regex or keyword (escape backslashes)"); return r; }
                const bool lexeme=kind=="lexeme";
                r.registry.add_symbol({lexeme?parsetk::Symbol::Kind::Lexeme:parsetk::Symbol::Kind::Keyword,std::string(n),
                    lexeme?parsetk::PatternSpec::regex(*value):parsetk::PatternSpec::exact(*value),{}});
            } else { fail("unknown grammar declaration kind"); return r; }
        }
        if (end==source.size()) break;
        pos=end+1; ++line_no;
    }
    if (r.start.empty()) r.error="grammar has no start production";
    else { parsetk::Parser parser(r.registry,options); r.error=parser.grammar_error(); }
    return r;
}
} // namespace metachain::text
