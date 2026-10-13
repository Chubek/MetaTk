#include "DSLtk.hpp"
#include <cassert>
#include <iostream>

int main() {
  dsl::Preprocessor preprocessor;
  auto rules = dsl::sexpr_rules(preprocessor.process(R"(
#define ZERO 0
#define ONE 1
(rewrite add-zero (add ?x ZERO) ?x)
(rewrite mul-one (mul ?x ONE) ?x)
)"));
  dsl::EGraph graph;
  auto root = graph.add("(mul (add a 0) 1)");
  auto atom = graph.add("a");
  auto report = graph.saturate(rules);
  assert(report.saturated());
  graph.when_equal(root, atom, [](dsl::EGraph& graph, dsl::EClassId id) {
    auto tree = graph.extract_ast(id);
    auto visitor = dsl::compile_visitor(dsl::ast_schema<"atom">(),
      dsl::on<"atom">([](const dsl::ASTNode& n) { std::cout << n.value() << '\n'; }));
    visitor.walk(tree);
  });
  auto grammar = dsl::sexpr_grammar(R"((grammar (start Expr)
    (rule Expr (Expr "+" Term) (Term)) (rule Term ("number"))))");
  assert(grammar.parse({"number", "+", "number"}).ok());
  assert(dsl::StaticRegex<"(add|mul)-[a-z]+">::matches("add-zero"));
}
