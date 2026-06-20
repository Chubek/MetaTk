#include "ini-Parzek.hpp"
#include <algorithm>
#include <cctype>
#include <functional>
#include <ranges>
#include <sstream>
#include <type_traits>

namespace IniLike_parzek {

namespace parzek_support {
static bool eval_guard(PredicateContext& ctx, std::string_view expr) {
  std::string e(expr);
  auto has = [&](std::string_view p){ return e.find(p) != std::string::npos; };
  if (has("LEN")) {
    auto pos = e.find("!=");
    if (pos != std::string::npos) {
      auto rhs = std::stoll(e.substr(pos + 2));
      return static_cast<long long>(ctx.current_text.size()) != rhs;
    }
    pos = e.find("==");
    if (pos != std::string::npos) {
      auto rhs = std::stoll(e.substr(pos + 2));
      return static_cast<long long>(ctx.current_text.size()) == rhs;
    }
  }
  if (has("CHAN") && has("==")) {
    auto pos = e.find("==");
    auto rhs = e.substr(pos + 2);
    rhs.erase(std::remove_if(rhs.begin(), rhs.end(), [](unsigned char c){ return std::isspace(c); }), rhs.end());
    return ctx.chan == rhs;
  }
  if (has("$0") && has("!=")) {
    auto quote = e.find('\'');
    if (quote != std::string::npos && quote + 2 < e.size()) {
      auto ch = e[quote + 1];
      return ctx.current_text.empty() || ctx.current_text[0] != ch;
    }
  }
  return true;
}
template <typename T>
dsl::Parser<T> when_guard(const dsl::Parser<T>& p, PredicateContext& ctx, std::string_view expr) {
  return dsl::parser([p, &ctx, expr](dsl::ParsecInput& in)->dsl::ExpectedResult<T>{
    auto save = in.pos;
    auto r = p(in);
    if (!r) return r;
    if (!eval_guard(ctx, expr)) {
      in.pos = save;
      return dsl::ExpectedResult<T>::failure(save, dsl::ParseFailureKind::Soft, {"when"});
    }
    return r;
  });
}
dsl::Parser<std::string> token(std::string token_text, bool allow_skip, PredicateContext* ctx) {
  return dsl::parser([token_text = std::move(token_text), allow_skip, ctx](dsl::ParsecInput& in)->dsl::ExpectedResult<std::string>{
    auto start = in.pos;
    std::size_t skipped = 0;
    if (allow_skip) {
      while (!in.eof() && std::isspace(static_cast<unsigned char>(in.peek()))) { in.consume(); ++skipped; }
      if (ctx) ctx->chan = skipped > 0 ? "@IGNORE" : "@TOK";
    }
    for (char c : token_text) {
      if (in.eof() || in.peek() != c) {
        in.pos = start;
        if (ctx) ctx->chan = "@TOK";
        return dsl::ExpectedResult<std::string>::failure(start, dsl::ParseFailureKind::Soft, {token_text});
      }
      in.consume();
    }
    if (ctx) { ctx->current_text = token_text; if (!allow_skip) ctx->chan = "@TOK"; }
    return token_text;
  });
}
dsl::Parser<std::string> char_class(std::string chars) {
  return dsl::parser([chars = std::move(chars)](dsl::ParsecInput& in)->dsl::ExpectedResult<std::string>{
    if (in.eof()) return dsl::ExpectedResult<std::string>::failure(in.pos, dsl::ParseFailureKind::Soft, {"char-class"});
    char c = in.peek();
    bool ok = false;
    for (std::size_t i = 0; i < chars.size(); ++i) {
      if (i + 2 < chars.size() && chars[i + 1] == '-') {
        if (c >= chars[i] && c <= chars[i + 2]) ok = true;
        i += 2;
      } else if (chars[i] == c) ok = true;
    }
    if (!ok) return dsl::ExpectedResult<std::string>::failure(in.pos, dsl::ParseFailureKind::Soft, {"char-class"});
    in.consume();
    return std::string(1, c);
  });
}
template <typename T> dsl::Parser<std::string> lift(const dsl::Parser<T>& p) {
  return dsl::parser([p](dsl::ParsecInput& in)->dsl::ExpectedResult<std::string>{
    auto r = p(in);
    if (!r) return dsl::ExpectedResult<std::string>::failure(r.error.pos, r.error.kind, r.error.expected);
    std::ostringstream ss;
    if constexpr (std::is_same_v<T, std::string>) { ss << *r; }
    else if constexpr (std::is_same_v<T, char>) { ss << *r; }
    else if constexpr (std::is_same_v<T, std::vector<std::string>>) { for (const auto& it : *r) ss << it; }
    else if constexpr (std::is_same_v<T, std::optional<std::string>>) { if (r->has_value()) ss << r->value(); }
    else { ss << "<node>"; }
    return ss.str();
  });
}
dsl::Parser<std::string> seq(const dsl::Parser<std::string>& a, const dsl::Parser<std::string>& b) {
  return dsl::parser([a, b](dsl::ParsecInput& in)->dsl::ExpectedResult<std::string>{
    auto r = (a & b)(in);
    if (!r) return dsl::ExpectedResult<std::string>::failure(r.error.pos, r.error.kind, r.error.expected);
    return (*r).first + (*r).second;
  });
}
template <typename T> dsl::Parser<std::vector<T>> many1(const dsl::Parser<T>& p) {
  return dsl::parser([p](dsl::ParsecInput& in)->dsl::ExpectedResult<std::vector<T>>{
    auto first = p(in);
    if (!first) return dsl::ExpectedResult<std::vector<T>>::failure(first.error.pos, first.error.kind, first.error.expected);
    std::vector<T> out;
    out.push_back(*first);
    while (true) {
      auto next = p(in);
      if (!next) break;
      out.push_back(*next);
    }
    return out;
  });
}
template <typename T, typename Open, typename Close> dsl::Parser<T> between(const dsl::Parser<Open>& open, const dsl::Parser<T>& core, const dsl::Parser<Close>& close) {
  return dsl::parser([open, core, close](dsl::ParsecInput& in)->dsl::ExpectedResult<T>{
    auto left = open(in); if (!left) return dsl::ExpectedResult<T>::failure(left.error.pos, left.error.kind, left.error.expected);
    auto mid = core(in); if (!mid) return dsl::ExpectedResult<T>::failure(mid.error.pos, mid.error.kind, mid.error.expected);
    auto right = close(in); if (!right) return dsl::ExpectedResult<T>::failure(right.error.pos, right.error.kind, right.error.expected);
    return *mid;
  });
}
template <typename T, typename Sep> dsl::Parser<std::vector<T>> sep_by(const dsl::Parser<T>& item, const dsl::Parser<Sep>& sep) {
  return dsl::parser([item, sep](dsl::ParsecInput& in)->dsl::ExpectedResult<std::vector<T>>{
    auto save = in.pos;
    auto one = sep_by1(item, sep)(in);
    if (!one) {
      in.pos = save;
      return std::vector<T>{};
    }
    return *one;
  });
}
template <typename T, typename Sep> dsl::Parser<std::vector<T>> sep_by1(const dsl::Parser<T>& item, const dsl::Parser<Sep>& sep) {
  return dsl::parser([item, sep](dsl::ParsecInput& in)->dsl::ExpectedResult<std::vector<T>>{
    std::vector<T> out;
    auto first = item(in);
    if (!first) return dsl::ExpectedResult<std::vector<T>>::failure(first.error.pos, first.error.kind, first.error.expected);
    out.push_back(*first);
    while (true) {
      auto save = in.pos;
      auto s = sep(in);
      if (!s) { in.pos = save; break; }
      auto v = item(in);
      if (!v) { in.pos = save; break; }
      out.push_back(*v);
    }
    return out;
  });
}
}

Parser::Parser(std::string source_name) { ctx_.file = std::move(source_name); }

dsl::Parser<std::string> Parser::skip_ignored() {
  return dsl::parser([this](dsl::ParsecInput& in)->dsl::ExpectedResult<std::string>{
    std::string out;
    while (!in.eof() && std::isspace(static_cast<unsigned char>(in.peek()))) out.push_back(in.consume());
    ctx_.chan = "@IGNORE";
    return out;
  });
}

dsl::Parser<std::string> Parser::parse_WS() {
  return parzek_support::lift(parzek_support::many1(parzek_support::char_class(" \\t\\r\\n")));
}

dsl::Parser<std::string> Parser::parse_LBRACKET() {
  return parzek_support::lift(parzek_support::token("[", false, &ctx_));
}

dsl::Parser<std::string> Parser::parse_RBRACKET() {
  return parzek_support::lift(parzek_support::token("]", false, &ctx_));
}

dsl::Parser<std::string> Parser::parse_EQ() {
  return parzek_support::lift(parzek_support::token("=", false, &ctx_));
}

dsl::Parser<std::string> Parser::parse_KEY() {
  return parzek_support::lift(parzek_support::many1(parzek_support::char_class("a-zA-Z_")));
}

dsl::Parser<std::string> Parser::parse_VALUE() {
  return parzek_support::lift(parzek_support::many1(parzek_support::char_class("a-zA-Z0-9_")));
}

dsl::Parser<std::string> Parser::parse_section() {
  return parzek_support::lift(parzek_support::seq(parzek_support::seq(parzek_support::lift(parse_LBRACKET()), parzek_support::lift(parse_KEY())), parzek_support::lift(parse_RBRACKET())));
}

dsl::Parser<std::string> Parser::parse_entry() {
  return parzek_support::lift(parzek_support::seq(parzek_support::seq(parzek_support::lift(parse_KEY()), parzek_support::lift(parse_EQ())), parzek_support::lift(parse_VALUE())));
}

dsl::Parser<std::string> Parser::parse_config() {
  return parzek_support::lift(parzek_support::seq(parzek_support::lift(parse_section()), parzek_support::lift(*parse_entry())));
}

dsl::ParseOutcome<std::string> Parser::parse(std::string_view input) {
  return dsl::run_parser(parse_config(), input);
}

std::shared_ptr<ASTNode> Parser::parse_ast(std::string_view input) {
  dsl::ParsecInput in(input);
  active_input_ = &in;
  auto result = parse_config_ast();
  active_input_ = nullptr;
  return result;
}

std::shared_ptr<ASTNode> Parser::parse_section_ast() {
  auto node = std::make_shared<ASTNode>();
  node->rule_name = "section";
  node->line = ctx_.line;
  node->column = ctx_.column;
  auto result = parse_section()(*active_input_);
  if (result) {
    node->text = *result;
  }
  return node;
}

std::shared_ptr<ASTNode> Parser::parse_entry_ast() {
  auto node = std::make_shared<ASTNode>();
  node->rule_name = "entry";
  node->line = ctx_.line;
  node->column = ctx_.column;
  auto result = parse_entry()(*active_input_);
  if (result) {
    node->text = *result;
  }
  return node;
}

std::shared_ptr<ASTNode> Parser::parse_config_ast() {
  auto node = std::make_shared<ASTNode>();
  node->rule_name = "config";
  node->line = ctx_.line;
  node->column = ctx_.column;
  auto result = parse_config()(*active_input_);
  if (result) {
    node->text = *result;
  }
  return node;
}

}
