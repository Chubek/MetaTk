#pragma once

#include "DSLUtils.hpp"
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <memory>

namespace IniLike_parzek {

struct ASTNode {
  std::string rule_name;
  std::string text;
  std::vector<std::shared_ptr<ASTNode>> children;
  size_t line{1};
  size_t column{1};
};

struct PredicateContext {
  std::string file;
  std::size_t line{1};
  std::size_t column{1};
  std::string chan{"@IGNORE"};
  std::string current_text;
};

namespace parzek_support {
template <typename T>
dsl::Parser<T> when_guard(const dsl::Parser<T>& p, PredicateContext& ctx, std::string_view expr);
dsl::Parser<std::string> token(std::string token_text, bool allow_skip, PredicateContext* ctx = nullptr);
dsl::Parser<std::string> char_class(std::string chars);
template <typename T> dsl::Parser<std::string> lift(const dsl::Parser<T>& p);
dsl::Parser<std::string> seq(const dsl::Parser<std::string>& a, const dsl::Parser<std::string>& b);
template <typename T> dsl::Parser<std::vector<T>> many1(const dsl::Parser<T>& p);
template <typename T, typename Open, typename Close> dsl::Parser<T> between(const dsl::Parser<Open>& open, const dsl::Parser<T>& core, const dsl::Parser<Close>& close);
template <typename T, typename Sep> dsl::Parser<std::vector<T>> sep_by(const dsl::Parser<T>& item, const dsl::Parser<Sep>& sep);
template <typename T, typename Sep> dsl::Parser<std::vector<T>> sep_by1(const dsl::Parser<T>& item, const dsl::Parser<Sep>& sep);
}

struct Parser {
  explicit Parser(std::string source_name = "<input>");
  dsl::ParseOutcome<std::string> parse(std::string_view input);
  std::shared_ptr<ASTNode> parse_ast(std::string_view input);

  dsl::Parser<std::string> parse_WS();
  dsl::Parser<std::string> parse_LBRACKET();
  dsl::Parser<std::string> parse_RBRACKET();
  dsl::Parser<std::string> parse_EQ();
  dsl::Parser<std::string> parse_KEY();
  dsl::Parser<std::string> parse_VALUE();
  dsl::Parser<std::string> parse_section();
  dsl::Parser<std::string> parse_entry();
  dsl::Parser<std::string> parse_config();

  std::shared_ptr<ASTNode> parse_section_ast();
  std::shared_ptr<ASTNode> parse_entry_ast();
  std::shared_ptr<ASTNode> parse_config_ast();

 private:
  PredicateContext ctx_;
  dsl::ParsecInput* active_input_{nullptr};
  dsl::Parser<std::string> skip_ignored();
};

}
