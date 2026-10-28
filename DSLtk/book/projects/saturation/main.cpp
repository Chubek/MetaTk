#include "DSLtk.hpp"
#include <iostream>

int evaluate(const dsl::SExpr& e, int a) {
  if (e.is_atom) {
    if (e.text == "a") return a;
    if (e.text == "0") return 0;
    if (e.text == "1") return 1;
    throw std::invalid_argument("unknown atom");
  }
  if (e.items.size() != 3 || !e.items[0].is_atom)
    throw std::invalid_argument("expected binary expression");
  auto left = evaluate(e.items[1], a), right = evaluate(e.items[2], a);
  if (e.items[0].text == "add") return left + right;
  if (e.items[0].text == "mul") return left * right;
  throw std::invalid_argument("unknown operator");
}
int main() {
  auto rules = dsl::sexpr_rules(R"(
    (rewrite add-zero (add ?x 0) ?x)
    (rewrite mul-one (mul ?x 1) ?x)
  )");
  auto input = dsl::parse_sexpr("(mul (add a 0) 1)");
  dsl::EGraph graph;
  auto root = graph.add(input), atom = graph.add("a");
  std::size_t observations = 0;
  auto report = graph.saturate(rules, {20, 1000, 1000},
      [&](const dsl::ERewriteRule&, const dsl::EMatch&) { ++observations; });
  assert(report.saturated());
  assert(observations == report.applications);
  assert(graph.equivalent(root, atom));
  auto output = graph.extract(root);
  assert(output == dsl::parse_sexpr("a"));
  for (int a = -20; a <= 20; ++a) assert(evaluate(input, a) == evaluate(output, a));
  bool notified = false;
  graph.when_equal(root, atom, [&](dsl::EGraph&, dsl::EClassId) { notified = true; });
  assert(notified);
  dsl::EGraph limited;
  auto limited_root = limited.add(input);
  auto stopped = limited.saturate(rules, {0, 1000, 1000});
  assert(stopped.stop == dsl::SaturationStop::IterationLimit);
  assert(limited.extract(limited_root) == input);
  dsl::TermRewriteSystem directed;
  directed.add_rule("(rewrite add-zero (add ?x 0) ?x)");
  directed.add_rule("(rewrite mul-one (mul ?x 1) ?x)");
  auto normal = directed.normalize(dsl::parse_term(input.dump()));
  assert(normal.normal_form() && normal.term == dsl::parse_term("a"));
  std::cout << input.dump() << " => " << output.dump() << '\n';
  std::cout << "saturated=" << report.saturated()
            << " iterations=" << report.iterations
            << " applications=" << report.applications << '\n';
}
