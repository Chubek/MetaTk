#include "DSLtk.hpp"
#include <cassert>
#include <iostream>

using namespace dsl;

template<class Function> void rejects(Function function) {
  bool rejected = false;
  try { function(); } catch (const std::exception&) { rejected = true; }
  assert(rejected);
}

void graphs() {
  DirectedGraph<> graph;
  auto root = graph.add_node("root\"\\\n");
  auto left = graph.add_node("left"), right = graph.add_node("right");
  auto isolated = graph.add_node("isolated");
  auto first = graph.add_edge(root, left, "first");
  graph.add_edge(root, left, "parallel");
  graph.add_edge(left, right);
  assert(graph.node_count() == 4 && graph.edge_count() == 3);
  assert((graph.breadth_first(root) == std::vector{root, left, right}));
  assert((graph.depth_first(root, true) == std::vector{right, left, root}));
  assert(graph.topological_sort().size() == 4);
  assert(graph.strongly_connected_components().size() == 4);
  assert(graph.shortest_path(root, right) == std::vector({root, left, right}));
  assert(!graph.shortest_path(root, isolated));
  assert(graph.shortest_path(root, root) == std::vector{root});
  auto cycle = graph.add_edge(right, root);
  rejects([&] { graph.topological_sort(); });
  assert(graph.depth_first(root).size() == 3);
  assert(graph.strongly_connected_components().size() == 2);
  graph.remove_edge(cycle);
  graph.redirect(left, right);
  assert(graph.edge(first).target == right && graph.predecessors(right).size() == 3);
  graph.remove_node(left);
  assert(graph.edge_count() == 2 && graph.node_count() == 3);
  rejects([&] { graph.add_edge(root, left); });
  rejects([&] { graph.value(left); });
  assert(graph.add_node("new") > isolated);
  auto self = graph.add_edge(right, right);
  graph.remove_node(right);
  assert(!graph.contains_edge(self) && graph.edge_count() == 0);
  assert(graph.to_dot("quoted\"name").find("root\\\"\\\\\\n") != std::string::npos);
  rejects([] { dot_quote(std::string(1, '\0')); });
  DirectedGraph<int, int> numbers;
  auto number = numbers.add_node(42);
  numbers.add_edge(number, number, 7);
  assert(numbers.to_dot().find("42") != std::string::npos);
  assert(numbers.to_dot([](int value) { return std::to_string(value + 1); },
                        [](int value) { return std::to_string(value); }).find("43") != std::string::npos);
}

void terms() {
  auto pattern = parse_term("(pair ?x ?x)");
  auto value = parse_term("(pair (f a) (f a))");
  auto bindings = match_term(pattern, value);
  assert(bindings && bindings->at("x") == parse_term("(f a)"));
  assert(!match_term(pattern, parse_term("(pair a b)")));
  assert(substitute(pattern, *bindings) == value);
  assert(value.ground() && value.size() == 5 && pattern.variables().size() == 1);
  assert(parse_term(parse_term("(\"?literal\" \"?x\" ?x)").dump()) == parse_term("(\"?literal\" \"?x\" ?x)"));
  std::vector<std::size_t> position{1, 0};
  assert(value.at(position) == parse_term("a"));
  assert(value.replaced(position, parse_term("b")) == parse_term("(pair (f a) (f b))"));
  assert(value == parse_term("(pair (f a) (f a))"));
  rejects([&] { value.at(std::vector<std::size_t>{8}); });
  rejects([] { parse_term("()"); });
  rejects([] { parse_term("(?f a)"); });
  rejects([] { parse_term("?"); });
  rejects([] { Term::variable(std::string(1, '\0')); });
  auto unified = unify_terms(parse_term("(f ?x (g ?y))"), parse_term("(f (g a) (g b))"));
  assert(unified && substitute(parse_term("(f ?x (g ?y))"), *unified) == parse_term("(f (g a) (g b))"));
  assert(!unify_terms(parse_term("?x"), parse_term("(f ?x)")));
  assert(!unify_terms(parse_term("(f ?x ?x)"), parse_term("(f a b)")));
  auto chained = unify_terms(parse_term("(f ?x ?y)"), parse_term("(f ?y a)"));
  assert(chained && substitute(parse_term("(f ?x ?y)"), *chained) == parse_term("(f a a)"));
}

void rewriting() {
  TermRewriteSystem system;
  system.add_rule("(rewrite add-zero (add ?x 0) ?x)");
  system.add_rule("(= (mul ?x 1) ?x)");
  auto input = parse_term("(mul (add a 0) 1)");
  auto outer = system.rewrite_once(input);
  auto inner = system.rewrite_once(input, RewriteStrategy::Innermost);
  assert(outer && outer->rule_index == 1 && outer->position.empty());
  assert(inner && inner->rule_index == 0 && inner->position == std::vector<std::size_t>{0});
  auto report = system.normalize(input, 2);
  assert(report.normal_form() && report.steps == 2 && report.term == parse_term("a"));
  assert(system.normalize(input, 0).stop == ReductionStop::StepLimit);
  assert(system.normalize(parse_term("a"), 0).normal_form());
  rejects([&] { system.add_rule("(= ?x a)"); });
  rejects([&] { system.add_rule("(= (f ?x) ?y)"); });
  TermRewriteSystem looping;
  looping.add_rule("(= a a)");
  assert(looping.normalize(parse_term("a"), 3).steps == 3);
  assert(looping.normalize(parse_term("a"), 3).stop == ReductionStop::StepLimit);
  TermRewriteSystem choose;
  choose.add_rule("(= (f ?x) done)");
  choose.add_rule("(= a b)");
  assert(choose.rewrite_once(parse_term("(f a)"))->term == parse_term("done"));
  assert(choose.rewrite_once(parse_term("(f a)"), RewriteStrategy::Innermost)->term == parse_term("(f b)"));
}

void reduction() {
  TermRewriteSystem system;
  system.add_rule("(= (add ?x 0) ?x)");
  TermGraph graph;
  auto root = graph.add("(pair (add a 0) (add a 0))");
  assert(graph.graph().node_count() == 4);
  auto children = graph.graph().successors(root);
  assert(children[0] == children[1]);
  auto before = graph.to_dot();
  assert(graph.reduce(root, system, 0).stop == ReductionStop::StepLimit);
  assert(graph.to_dot() == before);
  auto report = graph.reduce(root, system, 1);
  assert(report.normal_form() && report.steps == 1);
  assert(graph.to_term(report.root) == parse_term("(pair a a)"));
  assert(graph.add("(pair a a)") == report.root);
  assert(!graph.graph().contains(children[0]));
  assert(system.normalize(parse_term("(pair (add a 0) (add a 0))")).steps == 2);
  graph.collect_garbage({report.root});
  assert(graph.graph().node_count() == 2);
  TermGraph unshared;
  auto unshared_root = unshared.add("(pair (add a 0) (add a 0))", false);
  assert(unshared.reduce(unshared_root, system).steps == 2);
  TermRewriteSystem duplicate;
  duplicate.add_rule("(= (dup ?x) (pair ?x ?x))");
  TermGraph duplicate_graph;
  auto duplicate_result = duplicate_graph.reduce(duplicate_graph.add("(dup (f a))"), duplicate);
  auto duplicate_children = duplicate_graph.graph().successors(duplicate_result.root);
  assert(duplicate_children[0] == duplicate_children[1]);
  TermRewriteSystem nonlinear;
  nonlinear.add_rule("(= (pair ?x ?x) yes)");
  TermGraph equal;
  auto equal_result = equal.reduce(equal.add("(pair (f a) (f a))", false), nonlinear);
  assert(equal_result.normal_form() && equal.to_term(equal_result.root) == parse_term("yes"));
  TermGraph unequal;
  assert(unequal.reduce(unequal.add("(pair a b)"), nonlinear).steps == 0);
  TermRewriteSystem looping;
  looping.add_rule("(= a (f a))");
  TermGraph infinite;
  auto limited = infinite.reduce(infinite.add("a"), looping, 3);
  assert(limited.steps == 3 && limited.stop == ReductionStop::StepLimit);
  assert(infinite.to_term(limited.root) == parse_term("(f (f (f a)))"));
  TermRewriteSystem choose;
  choose.add_rule("(= (f ?x) done)");
  choose.add_rule("(= a b)");
  TermGraph inner_graph;
  auto inner = inner_graph.rewrite_once(inner_graph.add("(f a)"), choose, RewriteStrategy::Innermost);
  assert(inner && inner->rule_index == 1 && inner_graph.to_term(inner->root) == parse_term("(f b)"));
  TermGraph outer_graph;
  auto outer = outer_graph.rewrite_once(outer_graph.add("(f a)"), choose);
  assert(outer && outer->rule_index == 0 && outer_graph.to_term(outer->root) == parse_term("done"));
  auto retained = graph.to_dot();
  rejects([&] { graph.collect_garbage({report.root, 9999}); });
  assert(graph.to_dot() == retained);
  rejects([&] { graph.add_node("bad", {9999}); });
  graph.collect_garbage({});
  assert(graph.graph().node_count() == 0);
}

struct SymbolicDSL : DSL<SymbolicDSL, Graphs, Terms, TermRewriting, GraphReduction> {};
static_assert(HasFeature<SymbolicDSL, GraphReduction>);

int main(int argc, char**) {
  graphs(); terms(); rewriting(); reduction();
  if (argc > 1) {
    DirectedGraph<> graph;
    auto root = graph.add_node("quote\" slash\\ newline\n tab\t café");
    auto child = graph.add_node("isolated-looking node");
    graph.add_edge(root, child, "edge\"\\\n");
    graph.add_edge(root, child, "parallel");
    graph.add_edge(child, child, "loop");
    std::cout << graph.to_dot("graph\"name");
  }
}
