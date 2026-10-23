module test_graph_terms;

import DSLtk;
import std.algorithm : canFind;
import std.exception : assertThrown;

unittest
{
    auto graph = new DirectedGraph!();
    auto root = graph.add_node("root\"\\\n");
    auto left = graph.add_node("left"), right = graph.add_node("right");
    auto isolated = graph.add_node("isolated");
    auto first = graph.add_edge(root, left, "first");
    graph.add_edge(root, left, "parallel");
    graph.add_edge(left, right);
    assert(graph.node_count() == 4 && graph.edge_count() == 3);
    assert(graph.breadth_first(root) == [root, left, right]);
    assert(graph.depth_first(root, true) == [right, left, root]);
    assert(graph.topological_sort().length == 4);
    assert(graph.strongly_connected_components().length == 4);
    assert(graph.shortest_path(root, right).get == [root, left, right]);
    assert(graph.shortest_path(root, isolated).isNull);
    assert(graph.shortest_path(root, root).get == [root]);
    auto cycle = graph.add_edge(right, root);
    assertThrown(graph.topological_sort());
    assert(graph.depth_first(root).length == 3);
    assert(graph.strongly_connected_components().length == 2);
    graph.remove_edge(cycle);
    graph.redirect(left, right);
    assert(graph.edge(first).target == right && graph.predecessors(right).length == 3);
    graph.remove_node(left);
    assert(graph.edge_count() == 2 && graph.node_count() == 3);
    assertThrown(graph.add_edge(root, left));
    assertThrown(graph.value(left));
    assert(graph.add_node("new") > isolated);
    auto self = graph.add_edge(right, right);
    graph.remove_node(right);
    assert(!graph.contains_edge(self) && graph.edge_count() == 0);
    assert(graph.to_dot("quoted\"name").canFind("root\\\"\\\\\\n"));
    assertThrown(dot_quote("\0"));
    auto numbers = new DirectedGraph!(int, int);
    auto number = numbers.add_node(42);
    numbers.add_edge(number, number, 7);
    assert(numbers.to_dot().canFind("42"));
    assert(numbers.to_dot((int value) => "number", (int value) => "edge").canFind("number"));
}

unittest
{
    auto pattern = parse_term("(pair ?x ?x)");
    auto value = parse_term("(pair (f a) (f a))");
    auto bindings = match_term(pattern, value);
    assert(!bindings.isNull && bindings.get["x"] == parse_term("(f a)"));
    assert(match_term(pattern, parse_term("(pair a b)")).isNull);
    assert(substitute(pattern, bindings.get) == value);
    assert(value.ground() && value.size() == 5 && pattern.variables().length == 1);
    assert(parse_term(parse_term(`("?literal" "?x" ?x)`).dump()) == parse_term(`("?literal" "?x" ?x)`));
    size_t[] position = [1, 0];
    assert(value.at(position) == parse_term("a"));
    assert(value.replaced(position, parse_term("b")) == parse_term("(pair (f a) (f b))"));
    auto copy = value;
    copy.arguments[0].arguments[0].symbol = "changed";
    assert(value == parse_term("(pair (f a) (f a))"));
    assertThrown(value.at([8]));
    assertThrown(parse_term("()"));
    assertThrown(parse_term("(?f a)"));
    assertThrown(parse_term("?"));
    assertThrown(Term.variable("\0"));
    auto unified = unify_terms(parse_term("(f ?x (g ?y))"), parse_term("(f (g a) (g b))"));
    assert(!unified.isNull && substitute(parse_term("(f ?x (g ?y))"), unified.get) == parse_term("(f (g a) (g b))"));
    assert(unify_terms(parse_term("?x"), parse_term("(f ?x)")).isNull);
    assert(unify_terms(parse_term("(f ?x ?x)"), parse_term("(f a b)")).isNull);
    auto chained = unify_terms(parse_term("(f ?x ?y)"), parse_term("(f ?y a)"));
    assert(!chained.isNull && substitute(parse_term("(f ?x ?y)"), chained.get) == parse_term("(f a a)"));
}

unittest
{
    auto system = new TermRewriteSystem;
    system.add_rule("(rewrite add-zero (add ?x 0) ?x)");
    system.add_rule("(= (mul ?x 1) ?x)");
    auto input = parse_term("(mul (add a 0) 1)");
    auto outer = system.rewrite_once(input);
    auto inner = system.rewrite_once(input, RewriteStrategy.Innermost);
    assert(!outer.isNull && outer.get.rule_index == 1 && !outer.get.position.length);
    assert(!inner.isNull && inner.get.rule_index == 0 && inner.get.position == [0]);
    auto report = system.normalize(input, 2);
    assert(report.normal_form() && report.steps == 2 && report.term == parse_term("a"));
    assert(system.normalize(input, 0).stop == ReductionStop.StepLimit);
    assert(system.normalize(parse_term("a"), 0).normal_form());
    assertThrown(system.add_rule("(= ?x a)"));
    assertThrown(system.add_rule("(= (f ?x) ?y)"));
    auto looping = new TermRewriteSystem;
    looping.add_rule("(= a a)");
    assert(looping.normalize(parse_term("a"), 3).steps == 3);
    assert(looping.normalize(parse_term("a"), 3).stop == ReductionStop.StepLimit);
    auto choose = new TermRewriteSystem;
    choose.add_rule("(= (f ?x) done)");
    choose.add_rule("(= a b)");
    assert(choose.rewrite_once(parse_term("(f a)")).get.term == parse_term("done"));
    assert(choose.rewrite_once(parse_term("(f a)"), RewriteStrategy.Innermost).get.term == parse_term("(f b)"));
}

unittest
{
    auto system = new TermRewriteSystem;
    system.add_rule("(= (add ?x 0) ?x)");
    auto graph = new TermGraph;
    auto root = graph.add("(pair (add a 0) (add a 0))");
    assert(graph.graph().node_count() == 4);
    auto children = graph.graph().successors(root);
    assert(children[0] == children[1]);
    auto before = graph.to_dot();
    assert(graph.reduce(root, system, 0).stop == ReductionStop.StepLimit);
    assert(graph.to_dot() == before);
    auto report = graph.reduce(root, system, 1);
    assert(report.normal_form() && report.steps == 1);
    assert(graph.to_term(report.root) == parse_term("(pair a a)"));
    assert(graph.add("(pair a a)") == report.root);
    assert(!graph.graph().contains(children[0]));
    assert(system.normalize(parse_term("(pair (add a 0) (add a 0))")).steps == 2);
    graph.collect_garbage([report.root]);
    assert(graph.graph().node_count() == 2);
    auto unshared = new TermGraph;
    auto unshared_root = unshared.add("(pair (add a 0) (add a 0))", false);
    assert(unshared.reduce(unshared_root, system).steps == 2);
    auto duplicate = new TermRewriteSystem;
    duplicate.add_rule("(= (dup ?x) (pair ?x ?x))");
    auto duplicate_graph = new TermGraph;
    auto duplicate_result = duplicate_graph.reduce(duplicate_graph.add("(dup (f a))"), duplicate);
    auto duplicate_children = duplicate_graph.graph().successors(duplicate_result.root);
    assert(duplicate_children[0] == duplicate_children[1]);
    auto nonlinear = new TermRewriteSystem;
    nonlinear.add_rule("(= (pair ?x ?x) yes)");
    auto equal = new TermGraph;
    auto equal_result = equal.reduce(equal.add("(pair (f a) (f a))", false), nonlinear);
    assert(equal_result.normal_form() && equal.to_term(equal_result.root) == parse_term("yes"));
    auto unequal = new TermGraph;
    assert(unequal.reduce(unequal.add("(pair a b)"), nonlinear).steps == 0);
    auto looping = new TermRewriteSystem;
    looping.add_rule("(= a (f a))");
    auto infinite = new TermGraph;
    auto limited = infinite.reduce(infinite.add("a"), looping, 3);
    assert(limited.steps == 3 && limited.stop == ReductionStop.StepLimit);
    assert(infinite.to_term(limited.root) == parse_term("(f (f (f a)))"));
    auto choose = new TermRewriteSystem;
    choose.add_rule("(= (f ?x) done)");
    choose.add_rule("(= a b)");
    auto inner_graph = new TermGraph;
    auto inner = inner_graph.rewrite_once(inner_graph.add("(f a)"), choose, RewriteStrategy.Innermost);
    assert(!inner.isNull && inner.get.rule_index == 1 && inner_graph.to_term(inner.get.root) == parse_term("(f b)"));
    auto outer_graph = new TermGraph;
    auto outer = outer_graph.rewrite_once(outer_graph.add("(f a)"), choose);
    assert(!outer.isNull && outer.get.rule_index == 0 && outer_graph.to_term(outer.get.root) == parse_term("done"));
    auto retained = graph.to_dot();
    assertThrown(graph.collect_garbage([report.root, 9999]));
    assert(graph.to_dot() == retained);
    assertThrown(graph.add_node("bad", [9999]));
    graph.collect_garbage(null);
    assert(graph.graph().node_count() == 0);
}

struct SymbolicDSL { mixin DSL!(Graphs, Terms, TermRewriting, GraphReduction); }
static assert(HasFeature!(SymbolicDSL, GraphReduction));
