#include "DSLtk.hpp"
#include <charconv>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <stdexcept>
using U = std::uint64_t;
U evaluate(const dsl::SExpr& x) {
  if (x.is_atom) {
    if (x.quoted || x.text.empty()) throw std::runtime_error("expected unsigned atom");
    U value{};
    auto r = std::from_chars(x.text.data(), x.text.data()+x.text.size(), value);
    if (r.ec != std::errc{} || r.ptr != x.text.data()+x.text.size())
      throw std::runtime_error("invalid unsigned integer");
    return value;
  }
  if (x.items.size()!=3 || !x.items[0].is_atom || x.items[0].quoted)
    throw std::runtime_error("expected binary operator");
  auto op=x.items[0].text;
  if (op!="add" && op!="mul") throw std::runtime_error("unknown operator");
  U a=evaluate(x.items[1]), b=evaluate(x.items[2]);
  return op=="add" ? a+b : a*b; // arithmetic modulo 2^64
}
int main(int argc,char** argv) {
  try {
    std::size_t budget=1000;
    if(argc>2) throw std::runtime_error("usage: formula [step-budget] < input");
    if(argc==2) {
      std::string text=argv[1];
      auto r=std::from_chars(text.data(),text.data()+text.size(),budget);
      if(r.ec!=std::errc{} || r.ptr!=text.data()+text.size())
        throw std::runtime_error("invalid budget");
    }
    std::ostringstream buffer; buffer << std::cin.rdbuf();
    auto original=dsl::parse_sexpr(buffer.str());
    U before=evaluate(original);
    dsl::TermRewriteSystem system;
    for(auto rule : {"(rewrite azr (add ?x 0) ?x)",
                     "(rewrite azl (add 0 ?x) ?x)",
                     "(rewrite mor (mul ?x 1) ?x)",
                     "(rewrite mol (mul 1 ?x) ?x)"}) system.add_rule(rule);
    auto reduced=system.normalize(dsl::term_from_sexpr(original),budget);
    U after=evaluate(dsl::parse_sexpr(reduced.term.dump()));
    if(before!=after) throw std::runtime_error("semantic regression");
    std::cout << reduced.term.dump() << '\n';
    std::cerr << "steps=" << reduced.steps << "; "
              << (reduced.normal_form()?"normal-form":"step-limit") << '\n';
    return reduced.normal_form()?0:2;
  } catch(const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
