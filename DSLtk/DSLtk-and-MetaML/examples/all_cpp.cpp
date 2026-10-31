#include "MetaML.hpp"
#include <cassert>
#include <iostream>
#include <cstdint>
#undef assert
#define assert(...) do { if (!(__VA_ARGS__)) throw std::runtime_error(#__VA_ARGS__); } while(0)
void ex01_01() {
struct Domain : dsl::DSL<Domain, dsl::Pipeline> {};
Domain domain;
auto stage = dsl::pipe([](int x) { return x + 1; });
assert((domain.wrap(41) | stage) == 42);
}
void ex01_02() {
auto twice = dsl::pipe([](int x) { return x * 2; });
auto answer = 21 | twice;
assert(answer == 42);
}
void ex01_03() {
metaml::Session s;
auto r = s.eval("40 + 2");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "42");
}
void ex01_04() {
metaml::Session s;
assert(s.eval("val base := 40").is_ok());
auto r = s.eval("base + 2");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "42");
}
void ex01_05() {
metaml::Session s;
auto r = s.type_of("42");
assert(r.is_ok());
assert(r.unwrap() == "Int");
}
void ex02_01() {
auto add = dsl::pipe([](int x) { return x + 3; });
auto mul = dsl::pipe([](int x) { return x * 4; });
assert((2 | add | mul) == 20);
}
void ex02_02() {
auto positive = dsl::predicate([](int x) { return x > 0; });
auto even = dsl::predicate([](int x) { return x % 2 == 0; });
auto valid = positive & even;
assert(valid(8));
assert(!valid(-2));
}
void ex02_03() {
auto zero = dsl::predicate([](int x) { return x == 0; });
auto nonzero = !zero;
assert(nonzero(7));
assert(!nonzero(0));
}
void ex02_04() {
auto table = dsl::match(
  dsl::when<1>([](int) { return 10; }),
  dsl::otherwise([](int) { return -1; }));
assert(table(1) == 10);
assert(table(2) == -1);
}
void ex02_05() {
assert(dsl::pattern<"[0-9]+">::matches("2048"));
assert(!dsl::pattern<"[0-9]+">::matches("20x"));
}
void ex03_01() {
auto n = dsl::leaf<"integer">(42);
assert(n.is_leaf());
assert(n.tag() == "integer");
assert(n.value() == "42");
}
void ex03_02() {
auto n = dsl::node<"sub">(
  dsl::leaf<"integer">(9), dsl::leaf<"integer">(4));
assert(!n.is_leaf());
assert(n.size() == 2);
assert(n.child(0).value() == "9");
}
void ex03_03() {
auto before = dsl::node<"box">(dsl::leaf<"n">(1));
auto after = before;
after.child(0) = dsl::leaf<"n">(2);
assert(before.child(0).value() == "1");
assert(after.child(0).value() == "2");
}
void ex03_04() {
auto a = dsl::leaf<"n">(1);
auto b = dsl::leaf<"n">(1);
auto c = dsl::leaf<"other">(1);
assert(a == b);
assert(!(a == c));
}
void ex03_05() {
auto n = dsl::node<"add">(dsl::leaf<"n">(1),
                           dsl::leaf<"n">(2));
assert(n.dump() == "(add (n 1) (n 2))");
}
void ex04_01() {
dsl::Maybe<int> x(4);
auto y = x.map([](int n) { return n * 3; });
assert(y.or_else(0) == 12);
assert(dsl::Maybe<int>{}.or_else(9) == 9);
}
void ex04_02() {
using R = dsl::Result<int, std::string>;
auto good = R::from_ok(42);
auto bad = R::from_err("invalid number");
assert(good.unwrap() == 42);
assert(bad.unwrap_err() == "invalid number");
}
void ex04_03() {
using R = dsl::Result<int, std::string>;
auto r = R::from_ok(6).map([](int n) { return n * 7; });
assert(r.is_ok());
assert(r.unwrap() == 42);
}
void ex04_04() {
int calls = 0;
dsl::Lazy<int> x([&] { ++calls; return 42; });
assert(!x.is_computed());
assert(x.get() == 42);
assert(x.force() == 42 && calls == 1);
}
void ex04_05() {
int calls = 0;
auto square = dsl::memoize<int, int>([&](int n) {
  ++calls; return n * n;
});
assert(square(6) == 36 && square(6) == 36);
assert(calls == 1);
square.clear_cache();
}
void ex05_01() {
auto p = dsl::ch('a');
assert(dsl::run_parser(p, "a").value.has_value());
assert(!dsl::run_parser(p, "ab").value.has_value());
}
void ex05_02() {
auto p = dsl::ch('a') & dsl::ch('b');
auto r = dsl::run_parser(p, "ab");
assert(r.value.has_value());
assert(r.value->first == 'a');
}
void ex05_03() {
auto ab = dsl::ch('a') & dsl::ch('b');
auto ac = dsl::ch('a') & dsl::ch('c');
auto p = dsl::try_parse(ab) | ac;
assert(dsl::run_parser(p, "ac").value.has_value());
}
void ex05_04() {
auto p = *dsl::ch('x');
auto r = dsl::run_parser(p, "xxx");
assert(r.value.has_value());
assert(r.value->size() == 3);
}
void ex05_05() {
auto p = dsl::optional(dsl::ch('x'));
auto r = dsl::run_parser(p, "");
assert(r.value.has_value());
assert(!r.value->has_value());
}
void ex06_01() {
auto g = dsl::create_peg_definition();
g.add_rule<"[0-9]+">();
assert(g.parse("123").ok());
assert(!g.parse("12x").ok());
}
void ex06_02() {
int count = 0;
auto g = dsl::create_peg_definition();
g.add_rule<"[0-9]+">([&](const dsl::PEGMatch& m) {
  assert(m.value == "42"); ++count;
});
assert(g.parse("42").ok() && count == 1);
}
void ex06_03() {
auto g = dsl::create_peg_definition();
g.add_rule<"ab">();
g.add_rule<"a">();
assert(g.parse("ab").ok());
}
void ex06_04() {
auto g = dsl::create_peg_definition();
g.add_rule<"[0-9]+">();
auto r = g.parse("12?");
assert(r.failed());
assert(r.error_offset() == 2);
}
void ex06_05() {
auto g = dsl::create_peg_definition();
g.add_rule<"[0-9]*">();
auto r = g.parse("x");
assert(r.failed());
assert(r.error_offset() == 0);
}
void ex07_01() {
auto s = dsl::parse_sexpr("(add 1 2)");
assert(!s.is_atom);
assert(s.items.size() == 3);
assert(s.dump() == "(add 1 2)");
}
void ex07_02() {
auto s = dsl::SExpr::atom("two words");
auto text = s.dump();
auto back = dsl::parse_sexpr(text);
assert(back.text == "two words");
assert(back.quoted);
}
void ex07_03() {
auto xs = dsl::parse_sexpressions("(a 1) (b 2)");
assert(xs.size() == 2);
assert(xs[1].items[0].text == "b");
}
void ex07_04() {
auto n = dsl::leaf<"name">("two words");
auto sexpr = dsl::ast_to_sexpr(n);
auto back = dsl::sexpr_to_ast(sexpr);
assert(back == n);
}
void ex07_05() {
auto r = dsl::sexpr_rule(
  "(rewrite add-zero (add ?x 0) ?x)");
assert(r.name == "add-zero");
assert(r.rhs.text == "?x");
}
void ex08_01() {
dsl::DirectedGraph<> g;
auto a = g.add_node("parse");
auto b = g.add_node("check");
g.add_edge(a, b, "next");
assert(g.successors(a).front() == b);
}
void ex08_02() {
dsl::DirectedGraph<> g;
auto a = g.add_node("input");
auto b = g.add_node("output");
g.add_edge(a, b);
auto order = g.topological_sort();
assert(order.front() == a && order.back() == b);
}
void ex08_03() {
auto p = dsl::parse_term("(pair ?x ?x)");
auto good = dsl::parse_term("(pair a a)");
auto bad = dsl::parse_term("(pair a b)");
assert(dsl::match_term(p, good).has_value());
assert(!dsl::match_term(p, bad).has_value());
}
void ex08_04() {
dsl::TermRewriteSystem rs;
rs.add_rule("(rewrite z (add ?x 0) ?x)");
auto r = rs.normalize(dsl::parse_term("(add a 0)"), 20);
assert(r.normal_form());
assert(r.term.dump() == "a");
}
void ex08_05() {
dsl::TermGraph g;
auto root = g.add("(pair (f a) (f a))");
assert(g.to_term(root).dump() == "(pair (f a) (f a))");
assert(g.graph().node_count() == 3);
}
void ex09_01() {
dsl::EGraph g;
auto a = g.add("(add x 0)");
auto b = g.add("(add x 0)");
assert(g.equivalent(a, b));
}
void ex09_02() {
dsl::EGraph g;
auto a = g.add("(add x 0)");
auto b = g.add("x");
g.unite(a, b);
assert(g.equivalent(a, b));
}
void ex09_03() {
dsl::EGraph g;
auto root = g.add("(add x 0)");
auto rules = dsl::sexpr_rules("(rewrite z (add ?x 0) ?x)");
auto report = g.saturate(rules);
assert(report.saturated());
assert(g.extract(root).dump() == "x");
}
void ex09_04() {
dsl::EGraph g;
g.add("x");
dsl::SaturationLimits limits;
limits.iterations = 0;
auto r = g.saturate({}, limits);
assert(r.stop == dsl::SaturationStop::IterationLimit);
}
void ex09_05() {
dsl::EGraph g;
auto large = g.add("(id x)");
auto small = g.add("x");
g.unite(large, small);
assert(g.extract(large).dump() == "x");
}
void ex10_01() {
using Number = dsl::StaticRegex<"[0-9]+">;
assert(Number::matches("123"));
assert(!Number::matches("123x"));
}
void ex10_02() {
dsl::BinaryStream s;
s.write_integer<std::uint16_t>(0x1234, dsl::ByteOrder::Big);
assert(s.bytes.size() == 2);
assert(s.read_integer<std::uint16_t>(dsl::ByteOrder::Big) == 0x1234);
}
void ex10_03() {
dsl::BinaryStream s;
s.write_integer<std::uint32_t>(7);
auto bytes = s.read(4);
assert(bytes.size() == 4);
assert(s.eof());
}
void ex10_04() {
dsl::CFG g;
g.start = "S";
g.add("S", {dsl::CFGSymbol::token("name")});
assert(g.parse({"name"}).ok());
assert(!g.parse({"other"}).ok());
}
void ex10_05() {
dsl::CFG g;
g.start = "S";
g.add("S", {});
assert(g.parse({}).ok());
assert(!g.parse({"x"}).ok());
}
void ex11_01() {
dsl::Preprocessor p;
p.define("LIMIT", "42");
assert(p.expand("LIMIT + 1") == "42 + 1");
}
void ex11_02() {
dsl::Preprocessor p;
p.define_function("TWICE", {"x"}, "(x + x)");
assert(p.expand("TWICE(3)") == "(3 + 3)");
}
void ex11_03() {
dsl::Span a{0, 2, 1, 1, "sample.dsl"};
dsl::Span b{3, 5, 1, 4, "sample.dsl"};
auto all = dsl::merge_spans(a, b);
assert(all.begin == 0 && all.end == 5);
}
void ex11_04() {
std::string source = "one\ntwo";
dsl::SourceMap map(source);
auto pos = map.line_column(4);
assert(pos.first == 2 && pos.second == 1);
assert(map.line_text(2) == "two");
}
void ex11_05() {
dsl::Diagnostic d;
d.code = "APP001";
d.message = "expected a positive size";
assert(d.headline() == "error[APP001]: expected a positive size");
}
void ex12_01() {
auto n = dsl::leaf<"n">(42);
int seen = 0;
auto v = dsl::derive_visitor(n,
  dsl::on<"n">([&](const dsl::ASTNode&) { ++seen; }));
v.walk(n);
assert(seen == 1);
}
void ex12_02() {
auto units = dsl::literal_set(
  dsl::lit<"_km">([](long double x) { return x * 1000; }));
assert(units.parse("2_km") == 2000);
}
void ex12_03() {
enum class Mode { Read, Write };
assert(dsl::enum_name(Mode::Read) == "Read");
assert(dsl::enum_cast<Mode>("Write").value() == Mode::Write);
}
void ex12_04() {
auto schema = dsl::leaf<"n">(0);
auto v = dsl::derive_visitor(schema,
  dsl::on<"n">([](const dsl::ASTNode&) {}));
auto n = dsl::leaf<"n">(8);
v.visit(n);
assert(n.value() == "8");
}
void ex12_05() {
auto metres = [](std::string_view text) {
  auto ls = dsl::literal_set(dsl::lit<"_m">(
    [](long double n) { return n; }));
  return ls.parse(text);
};
assert(metres("12_m") == 12);
}
void ex13_01() {
metaml::Session s;
auto r = s.eval("42");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "42");
}
void ex13_02() {
metaml::Session s;
auto r = s.eval("unbound_name + 1");
assert(r.is_err());
assert(!r.unwrap_err().empty());
}
void ex13_03() {
metaml::Session s;
auto r = s.eval("val x := 6\nval y := 7\nx * y");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "42");
}
void ex13_04() {
metaml::Session s;
auto r = s.eval("val default_limit := 42\ndefault_limit");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "42");
}
void ex13_05() {
metaml::Session s;
auto r = s.eval("val local_only := 42\nlocal_only");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "42");
}
void ex14_01() {
metaml::Session s;
auto r = s.eval("2 + 5 * 8");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "42");
}
void ex14_02() {
metaml::Session s;
auto r = s.eval("1.5 + 2.25");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "3.75");
}
void ex14_03() {
metaml::Session s;
auto r = s.eval("String.length \"hello\"");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "5");
}
void ex14_04() {
metaml::Session s;
auto r = s.eval("(1, true, \"ok\")");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "(1, true, \"ok\")");
}
void ex14_05() {
metaml::Session s;
auto r = s.eval("[1, 2, 3]");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "[1, 2, 3]");
}
void ex15_01() {
metaml::Session s;
auto r = s.eval("(fn x -> x + 1) 41");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "42");
}
void ex15_02() {
metaml::Session s;
auto r = s.eval("val add := fn x y -> x + y\nval add40 := add 40\nadd40 2");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "42");
}
void ex15_03() {
metaml::Session s;
auto r = s.eval("let x = 6 in x * 7");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "42");
}
void ex15_04() {
metaml::Session s;
auto r = s.eval("val make := fn n -> fn x -> n * x\nval six := make 6\nsix 7");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "42");
}
void ex15_05() {
metaml::Session s;
auto r = s.eval("if 6 < 7 then 42 else 0");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "42");
}
void ex16_01() {
metaml::Session s;
auto r = s.eval("val id := fn x -> x\n(id 42, id true)");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "(42, true)");
}
void ex16_02() {
metaml::Session s;
auto r = s.eval("1 + true");
assert(r.is_err());
assert(!r.unwrap_err().empty());
}
void ex16_03() {
metaml::Session s;
auto r = s.eval("val r := {width = 6, height = 7}\nr.width * r.height");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "42");
}
void ex16_04() {
metaml::Session s;
auto r = s.eval("val width := fn r -> r.width\nwidth {width = 42, label = \"panel\"}");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "42");
}
void ex16_05() {
metaml::Session s;
auto r = s.eval("val r := {width = 1, height = 7}\nval q := {r with width = 6}\nq.width * q.height");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "42");
}
void ex17_01() {
metaml::Session s;
auto r = s.eval("match Some 42 with | Some x -> x | None -> 0");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "42");
}
void ex17_02() {
metaml::Session s;
auto r = s.eval("match Ok 42 with | Ok x -> x | Err e -> 0");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "42");
}
void ex17_03() {
metaml::Session s;
auto r = s.eval("type Answer := | Missing | Found Int\nmatch Found 42 with | Found n -> n | Missing -> 0");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "42");
}
void ex17_04() {
metaml::Session s;
auto r = s.eval("match Some 42 with | Some n when n > 0 -> n | _ -> 0");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "42");
}
void ex17_05() {
metaml::Session s;
auto r = s.eval("match [42, 7] with | h :: t -> h | [] -> 0");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "42");
}
void ex18_01() {
metaml::Session s;
auto r = s.eval("module Numbers := struct\nval answer := 42\nend\nNumbers.answer");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "42");
}
void ex18_02() {
metaml::Session s;
auto r = s.eval("module Scale := struct\nval apply := fn n -> n * 6\nend\nScale.apply 7");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "42");
}
void ex18_03() {
metaml::Session s;
auto r = s.eval("module N := struct\nval answer := 42\nend\nopen N\nanswer");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "42");
}
void ex18_04() {
metaml::Session s;
auto r = s.eval("module Outer := struct\nmodule Inner := struct\nval answer := 42\nend\nend\nOuter.Inner.answer");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "42");
}
void ex18_05() {
metaml::Session s;
auto r = s.eval("module N : sig\nval answer : Int\nend := struct\nval answer := 42\nend\nN.answer");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "42");
}
void ex19_01() {
metaml::Session s;
s.register_primitive("Host.twice", [](std::int64_t n) { return n * 2; });
auto r = s.eval("Host.twice 21");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "42");
}
void ex19_02() {
metaml::Session s;
s.register_primitive("Host.add",
  [](std::int64_t a, std::int64_t b) { return a + b; });
auto r = s.eval("Host.add 19 23");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "42");
}
void ex19_03() {
metaml::Session s;
s.register_primitive("Host.allowed",
  [](std::int64_t n) { return n >= 18; });
auto r = s.eval("Host.allowed 20");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "true");
}
void ex19_04() {
metaml::Session s;
s.register_primitive("Host.label",
  [](std::string text) { return text + "!"; });
auto r = s.eval("Host.label \"ok\"");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "\"ok!\"");
}
void ex19_05() {
metaml::Session s;
s.register_primitive("Host.answer", []() -> std::int64_t { return 42; });
auto r = s.eval("Host.answer ()");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "42");
}
void ex20_01() {
metaml::Session s;
auto r = s.eval("missing", "policy.ml");
assert(r.is_err());
auto text = s.render(r.unwrap_err().front());
assert(!text.empty());
}
void ex20_02() {
auto r = metaml::parse_expression("(1 + 2", "bad.ml");
assert(r.is_err());
assert(!r.unwrap_err().empty());
}
void ex20_03() {
auto r = metaml::parse("val x := 42", "unit.ml");
assert(r.ok());
assert(r.program.items.size() == 1);
}
void ex20_04() {
metaml::Session s;
auto r = s.type_of("fn x -> x");
assert(r.is_ok());
assert(r.unwrap().find("->") != std::string::npos);
}
void ex20_05() {
metaml::Session s;
assert(s.eval("val size := 42").is_ok());
auto r = s.type_of("size");
assert(r.is_ok() && r.unwrap() == "Int");
}
void ex21_01() {
metaml::Session s;
s.set_primitive_allowlist({});
auto r = s.eval("1 + 2");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "3");
}
void ex21_02() {
metaml::Session s;
std::string captured;
s.set_output([&](std::string_view x) { captured += x; });
s.write("hello");
assert(captured == "hello");
}
void ex21_03() {
metaml::Session s;
int state = 0;
s.on_close([&] { state = 1; }, [&] { assert(state == 1); state = 2; });
s.close();
assert(state == 2);
}
void ex21_04() {
metaml::Session s;
int stops = 0;
s.on_close([&] { ++stops; });
s.close();
s.close();
assert(stops == 1);
}
void ex21_05() {
metaml::Session s;
s.set_max_depth(1000);
auto r = s.eval("40 + 2");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "42");
}
void ex22_01() {
metaml::Session s;
auto r = s.load_stdlib("__book_missing_metalib__");
assert(r.is_err());
assert(!s.stdlib_loaded());
}
void ex22_02() {
metaml::Session s;
s.add_search_path("modules");
auto paths = s.search_paths();
assert(!paths.empty());
assert(paths.front() == "modules");
}
void ex22_03() {
metaml::Session s;
auto path = s.resolve_import("__book_missing_module__");
assert(!path.has_value());
}
void ex22_04() {
metaml::Session s;
auto names = s.primitive_names();
assert(!names.empty());
assert(s.find_primitive("Int.parse") != nullptr);
}
void ex22_05() {
metaml::Session s;
auto r = s.eval("val square := fn x -> x * x\nsquare 6 + 6");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "42");
}
void ex23_01() {
auto x = dsl::parse_sexpr("(add 42 0)");
assert(!x.is_atom && x.items.size() == 3);
assert(x.items[0].text == "add");
assert(x.items[1].text == "42");
}
void ex23_02() {
dsl::TermRewriteSystem rs;
rs.add_rule("(rewrite az (add ?x 0) ?x)");
rs.add_rule("(rewrite mo (mul ?x 1) ?x)");
auto r = rs.normalize(dsl::parse_term("(mul (add 42 0) 1)"), 100);
assert(r.normal_form());
assert(r.term.dump() == "42");
}
void ex23_03() {
dsl::EGraph g;
auto root = g.add("(mul (add 42 0) 1)");
auto rules = dsl::sexpr_rules(
 "(rewrite az (add ?x 0) ?x) (rewrite mo (mul ?x 1) ?x)");
auto r = g.saturate(rules);
assert(r.saturated());
assert(g.extract(root).dump() == "42");
}
void ex23_04() {
dsl::TermRewriteSystem rs;
rs.add_rule("(rewrite az (add ?x 0) ?x)");
auto r = rs.normalize(dsl::parse_term("(add 42 0)"), 0);
assert(!r.normal_form());
assert(r.steps == 0);
assert(r.term.dump() == "(add 42 0)");
}
void ex23_05() {
auto before = dsl::parse_sexpr("(add 42 0)");
dsl::TermRewriteSystem rs;
rs.add_rule("(rewrite az (add ?x 0) ?x)");
auto r = rs.normalize(dsl::term_from_sexpr(before));
assert(r.term.dump() == "42");
assert(before.dump() == "(add 42 0)");
}
void ex24_01() {
metaml::Session s;
auto r = s.eval("module Policy := struct\nval allow := fn age -> age >= 18\nend\nPolicy.allow 20");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "true");
}
void ex24_02() {
metaml::Session s;
s.register_primitive("Request.age", []() -> std::int64_t { return 20; });
auto r = s.eval("Request.age () >= 18");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "true");
}
void ex24_03() {
metaml::Session candidate;
assert(candidate.eval("val allow := fn age -> age >= 18").is_ok());
auto type = candidate.type_of("allow 20");
assert(type.is_ok() && type.unwrap() == "Bool");
auto r = candidate.eval("allow 17");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "false");
}
void ex24_04() {
metaml::Session s;
s.register_primitive("Request.age", []() -> std::int64_t { return 20; });
s.set_primitive_allowlist({"Request.age"});
auto r = s.eval("Request.age () >= 18");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "true");
}
void ex24_05() {
metaml::Session active;
assert(active.eval("val allow := fn age -> age >= 18").is_ok());
metaml::Session candidate;
assert(candidate.eval("val allow := fn age -> age + true").is_err());
auto r = active.eval("allow 20");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "true");
}
int main() { int passed=0, failed=0;
try { ex01_01(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex01_01: " << e.what() << "\n"; }
try { ex01_02(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex01_02: " << e.what() << "\n"; }
try { ex01_03(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex01_03: " << e.what() << "\n"; }
try { ex01_04(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex01_04: " << e.what() << "\n"; }
try { ex01_05(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex01_05: " << e.what() << "\n"; }
try { ex02_01(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex02_01: " << e.what() << "\n"; }
try { ex02_02(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex02_02: " << e.what() << "\n"; }
try { ex02_03(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex02_03: " << e.what() << "\n"; }
try { ex02_04(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex02_04: " << e.what() << "\n"; }
try { ex02_05(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex02_05: " << e.what() << "\n"; }
try { ex03_01(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex03_01: " << e.what() << "\n"; }
try { ex03_02(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex03_02: " << e.what() << "\n"; }
try { ex03_03(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex03_03: " << e.what() << "\n"; }
try { ex03_04(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex03_04: " << e.what() << "\n"; }
try { ex03_05(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex03_05: " << e.what() << "\n"; }
try { ex04_01(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex04_01: " << e.what() << "\n"; }
try { ex04_02(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex04_02: " << e.what() << "\n"; }
try { ex04_03(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex04_03: " << e.what() << "\n"; }
try { ex04_04(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex04_04: " << e.what() << "\n"; }
try { ex04_05(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex04_05: " << e.what() << "\n"; }
try { ex05_01(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex05_01: " << e.what() << "\n"; }
try { ex05_02(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex05_02: " << e.what() << "\n"; }
try { ex05_03(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex05_03: " << e.what() << "\n"; }
try { ex05_04(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex05_04: " << e.what() << "\n"; }
try { ex05_05(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex05_05: " << e.what() << "\n"; }
try { ex06_01(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex06_01: " << e.what() << "\n"; }
try { ex06_02(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex06_02: " << e.what() << "\n"; }
try { ex06_03(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex06_03: " << e.what() << "\n"; }
try { ex06_04(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex06_04: " << e.what() << "\n"; }
try { ex06_05(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex06_05: " << e.what() << "\n"; }
try { ex07_01(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex07_01: " << e.what() << "\n"; }
try { ex07_02(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex07_02: " << e.what() << "\n"; }
try { ex07_03(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex07_03: " << e.what() << "\n"; }
try { ex07_04(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex07_04: " << e.what() << "\n"; }
try { ex07_05(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex07_05: " << e.what() << "\n"; }
try { ex08_01(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex08_01: " << e.what() << "\n"; }
try { ex08_02(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex08_02: " << e.what() << "\n"; }
try { ex08_03(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex08_03: " << e.what() << "\n"; }
try { ex08_04(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex08_04: " << e.what() << "\n"; }
try { ex08_05(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex08_05: " << e.what() << "\n"; }
try { ex09_01(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex09_01: " << e.what() << "\n"; }
try { ex09_02(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex09_02: " << e.what() << "\n"; }
try { ex09_03(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex09_03: " << e.what() << "\n"; }
try { ex09_04(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex09_04: " << e.what() << "\n"; }
try { ex09_05(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex09_05: " << e.what() << "\n"; }
try { ex10_01(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex10_01: " << e.what() << "\n"; }
try { ex10_02(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex10_02: " << e.what() << "\n"; }
try { ex10_03(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex10_03: " << e.what() << "\n"; }
try { ex10_04(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex10_04: " << e.what() << "\n"; }
try { ex10_05(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex10_05: " << e.what() << "\n"; }
try { ex11_01(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex11_01: " << e.what() << "\n"; }
try { ex11_02(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex11_02: " << e.what() << "\n"; }
try { ex11_03(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex11_03: " << e.what() << "\n"; }
try { ex11_04(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex11_04: " << e.what() << "\n"; }
try { ex11_05(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex11_05: " << e.what() << "\n"; }
try { ex12_01(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex12_01: " << e.what() << "\n"; }
try { ex12_02(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex12_02: " << e.what() << "\n"; }
try { ex12_03(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex12_03: " << e.what() << "\n"; }
try { ex12_04(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex12_04: " << e.what() << "\n"; }
try { ex12_05(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex12_05: " << e.what() << "\n"; }
try { ex13_01(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex13_01: " << e.what() << "\n"; }
try { ex13_02(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex13_02: " << e.what() << "\n"; }
try { ex13_03(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex13_03: " << e.what() << "\n"; }
try { ex13_04(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex13_04: " << e.what() << "\n"; }
try { ex13_05(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex13_05: " << e.what() << "\n"; }
try { ex14_01(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex14_01: " << e.what() << "\n"; }
try { ex14_02(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex14_02: " << e.what() << "\n"; }
try { ex14_03(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex14_03: " << e.what() << "\n"; }
try { ex14_04(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex14_04: " << e.what() << "\n"; }
try { ex14_05(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex14_05: " << e.what() << "\n"; }
try { ex15_01(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex15_01: " << e.what() << "\n"; }
try { ex15_02(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex15_02: " << e.what() << "\n"; }
try { ex15_03(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex15_03: " << e.what() << "\n"; }
try { ex15_04(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex15_04: " << e.what() << "\n"; }
try { ex15_05(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex15_05: " << e.what() << "\n"; }
try { ex16_01(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex16_01: " << e.what() << "\n"; }
try { ex16_02(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex16_02: " << e.what() << "\n"; }
try { ex16_03(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex16_03: " << e.what() << "\n"; }
try { ex16_04(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex16_04: " << e.what() << "\n"; }
try { ex16_05(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex16_05: " << e.what() << "\n"; }
try { ex17_01(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex17_01: " << e.what() << "\n"; }
try { ex17_02(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex17_02: " << e.what() << "\n"; }
try { ex17_03(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex17_03: " << e.what() << "\n"; }
try { ex17_04(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex17_04: " << e.what() << "\n"; }
try { ex17_05(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex17_05: " << e.what() << "\n"; }
try { ex18_01(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex18_01: " << e.what() << "\n"; }
try { ex18_02(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex18_02: " << e.what() << "\n"; }
try { ex18_03(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex18_03: " << e.what() << "\n"; }
try { ex18_04(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex18_04: " << e.what() << "\n"; }
try { ex18_05(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex18_05: " << e.what() << "\n"; }
try { ex19_01(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex19_01: " << e.what() << "\n"; }
try { ex19_02(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex19_02: " << e.what() << "\n"; }
try { ex19_03(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex19_03: " << e.what() << "\n"; }
try { ex19_04(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex19_04: " << e.what() << "\n"; }
try { ex19_05(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex19_05: " << e.what() << "\n"; }
try { ex20_01(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex20_01: " << e.what() << "\n"; }
try { ex20_02(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex20_02: " << e.what() << "\n"; }
try { ex20_03(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex20_03: " << e.what() << "\n"; }
try { ex20_04(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex20_04: " << e.what() << "\n"; }
try { ex20_05(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex20_05: " << e.what() << "\n"; }
try { ex21_01(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex21_01: " << e.what() << "\n"; }
try { ex21_02(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex21_02: " << e.what() << "\n"; }
try { ex21_03(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex21_03: " << e.what() << "\n"; }
try { ex21_04(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex21_04: " << e.what() << "\n"; }
try { ex21_05(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex21_05: " << e.what() << "\n"; }
try { ex22_01(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex22_01: " << e.what() << "\n"; }
try { ex22_02(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex22_02: " << e.what() << "\n"; }
try { ex22_03(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex22_03: " << e.what() << "\n"; }
try { ex22_04(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex22_04: " << e.what() << "\n"; }
try { ex22_05(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex22_05: " << e.what() << "\n"; }
try { ex23_01(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex23_01: " << e.what() << "\n"; }
try { ex23_02(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex23_02: " << e.what() << "\n"; }
try { ex23_03(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex23_03: " << e.what() << "\n"; }
try { ex23_04(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex23_04: " << e.what() << "\n"; }
try { ex23_05(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex23_05: " << e.what() << "\n"; }
try { ex24_01(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex24_01: " << e.what() << "\n"; }
try { ex24_02(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex24_02: " << e.what() << "\n"; }
try { ex24_03(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex24_03: " << e.what() << "\n"; }
try { ex24_04(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex24_04: " << e.what() << "\n"; }
try { ex24_05(); ++passed; } catch(const std::exception& e) { ++failed; std::cerr << "ex24_05: " << e.what() << "\n"; }
std::cout << passed << " passed, " << failed << " failed\n"; return failed ? 1 : 0; }
