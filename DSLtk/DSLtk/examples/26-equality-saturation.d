module equality_saturation_example;
import DSLtk;
import std.stdio : writeln;

void main()
{
    auto preprocessor = new Preprocessor;
    auto rules = sexpr_rules(preprocessor.process(`
#define ZERO 0
#define ONE 1
(rewrite add-zero (add ?x ZERO) ?x)
(rewrite mul-one (mul ?x ONE) ?x)
`));
    auto graph = new EGraph;
    auto root = graph.add("(mul (add a 0) 1)");
    auto atom = graph.add("a");
    assert(graph.saturate(rules).saturated());
    graph.when_equal(root, atom, (EGraph graph, EClassId id) {
        auto tree = graph.extract_ast(id);
        enum schema = leaf!"atom"("");
        auto visitor = compile_visitor!schema(on!"atom"((ref ASTNode n) { writeln(n.value()); }));
        visitor.walk(tree);
    });
    auto grammar = sexpr_grammar(`(grammar (start Expr)
        (rule Expr (Expr "+" Term) (Term)) (rule Term ("number")))`);
    assert(grammar.parse(["number", "+", "number"]).ok());
    assert(StaticRegex!"(add|mul)-[a-z]+".matches("add-zero"));
}
