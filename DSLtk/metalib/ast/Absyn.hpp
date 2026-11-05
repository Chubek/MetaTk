#pragma once
#include "../cpp/ASTBuilder.h"
#include <fstream>

namespace metalib {
namespace absyn_detail {
inline metaml::Value tree(const metaml::Program &p)
{
  return ast_detail::of_node(metaml::program_ast(p));
}
inline metaml::Value parse(const std::string &source, const std::string &file)
{
  auto p = metaml::parse(source, file);
  if (!p.ok()) return support::err(p.diagnostics.front().headline());
  return support::ok(tree(p.program));
}
inline std::string read_file(const std::string &path)
{
  if (path.find('\0') != std::string::npos) throw std::runtime_error("path contains NUL");
  std::ifstream f(path, std::ios::binary);
  if (!f) throw std::runtime_error("cannot open file: " + path);
  std::string text((std::istreambuf_iterator<char>(f)), {});
  if (f.bad()) throw std::runtime_error("cannot read file: " + path);
  return text;
}
}
inline void install_Visitor(metaml::Session &s)
{
  if (!s.find_primitive("ASTBuilder.leaf")) install_ASTBuilder(s);
}
inline void install_Absyn(metaml::Session &s)
{
  using namespace support;
  install_Visitor(s);
  if (s.find_primitive("Absyn.of_source")) return;
  prim(s, "Absyn.of_source", "String -> Result Ast String", 1,
    [](C &, V &a) { return absyn_detail::parse(S(a[0]), "<absyn>"); });
  prim(s, "Absyn.of_file", "String -> Result Ast String", 1, [](C &, V &a) {
    try { return absyn_detail::parse(absyn_detail::read_file(S(a[0])), S(a[0])); }
    catch (const std::exception &e) { return err(e.what()); }
  });
  prim(s, "Absyn.current", "Unit -> Result Ast String", 1, [](C &c, V &) {
    auto p = c.session.current_program();
    return p ? ok(absyn_detail::tree(*p)) : err("no current program");
  });
}
}
