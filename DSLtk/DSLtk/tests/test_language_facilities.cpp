#include "DSLtk.hpp"
#include <cassert>

using namespace dsl;

template<class F> void must_throw(F fn) {
  bool thrown = false;
  try { fn(); } catch (const std::exception&) { thrown = true; }
  assert(thrown);
}

struct MemoryBackend {
  std::vector<std::byte> data;
  std::size_t pos = 0;
  std::size_t read(std::span<std::byte> target) {
    auto n = std::min({target.size(), data.size() - pos, std::size_t(2)});
    std::copy_n(data.begin() + pos, n, target.begin()); pos += n; return n;
  }
  std::size_t write(std::span<const std::byte> source) {
    auto n = std::min(source.size(), std::size_t(2));
    data.insert(data.end(), source.begin(), source.begin() + n); return n;
  }
  void seek(std::size_t position) { pos = position; }
  std::size_t tell() { return pos; }
};
struct UserCodec {
  void serialize(const int& value, BinaryStream& output) { output.write_integer(static_cast<std::uint32_t>(value)); }
  template<class T> T deserialize(BinaryStream& input) { return static_cast<T>(input.read_integer<std::uint32_t>()); }
};

void expressions_and_egraphs() {
  auto expression = parse_sexpr("; comment\n(add \"hello world\" (num 2))");
  assert(parse_sexpr(expression.dump()) == expression);
  assert(parse_sexpr("\"a\\n\\\"b\"").text == "a\n\"b");
  assert(parse_sexpr("café").text == "café");
  assert(parse_sexpr(SExpr::atom("hello world").dump()) == SExpr::atom("hello world"));
  must_throw([] { parse_sexpr("(a"); });
  must_throw([] { parse_sexpr("a b"); });
  must_throw([] { sexpr_rule("(= ?x ?y)"); });
  auto tree = node<"add">(leaf<"num">(1), leaf<"num">(2));
  assert(sexpr_to_ast(ast_to_sexpr(tree)) == tree);
  EGraph graph;
  auto root = graph.add("(mul (add a 0) 1)");
  auto a = graph.add("a");
  std::size_t observed = 0;
  auto report = graph.saturate(sexpr_rules("(rewrite add-zero (add ?x 0) ?x) (= (mul ?x 1) ?x)"), {},
    [&](const ERewriteRule&, const EMatch&) { ++observed; });
  assert(report.saturated() && observed > 0 && graph.equivalent(root, a));
  assert(graph.extract(root).dump() == "a");
  bool triggered = false;
  assert(graph.when_equal(root, a, [&](EGraph&, EClassId) { triggered = true; }) && triggered);
  EGraph congruence;
  auto x = congruence.add("a"), y = congruence.add("b");
  auto fx = congruence.add("(f a)"), fy = congruence.add("(f b)");
  assert(!congruence.equivalent(fx, fy));
  congruence.unite(x, y);
  assert(congruence.equivalent(fx, fy) && congruence.node_count() == 3);
  congruence.add("(pair a b)");
  assert(congruence.matches(parse_sexpr("(pair ?x ?x)")).size() == 1);
  EGraph cyclic;
  auto atom = cyclic.add("a");
  assert(cyclic.saturate(sexpr_rules("(= ?x (wrap ?x))")).saturated());
  assert(cyclic.extract(atom).text == "a");
  EGraph bounded;
  bounded.add("a");
  assert(bounded.saturate(sexpr_rules("(= a (f b c))"), {10, 2, 100}).stop == SaturationStop::NodeLimit);
  assert(bounded.node_count() <= 2);
  assert(bounded.saturate(sexpr_rules("(= a b)"), {10, 100, 0}).stop == SaturationStop::MatchLimit);
}

void binary_io_and_visitors() {
  assert(StaticRegex<"^(ab|a)+[0-9]?$">::matches("aba7"));
  assert(!StaticRegex<"^(ab|a)+[0-9]?$">::matches("aba77"));
  assert(StaticRegex<"a*ab">::matches("aaab"));
  assert(StaticRegex<"(a?)*">::matches(""));
  std::vector<std::byte> binary{std::byte{0}, std::byte{255}, std::byte{65}, std::byte{66}};
  assert(StaticRegex<"\\x00[\\x80-\\xff]AB">::matches(binary));
  auto match = StaticRegex<"AB">::search(binary);
  assert(match && match->begin == 2 && match->end == 4);
  BinaryStream stream(binary);
  auto token = stream.next<"\\0[\\x80-\\xff]">();
  assert(token && token->size() == 2 && stream.pos == 2);
  assert(!stream.next<"X">() && stream.pos == 2);
  must_throw([&] { stream.next<"a*">(); });
  BinaryStream self(binary);
  self.write(self.read(2));
  assert(self.bytes.size() == 6 && self.bytes[4] == std::byte{0} && self.bytes[5] == std::byte{255});
  BinaryStream numbers;
  numbers.write_integer(std::uint32_t(0x01020304), ByteOrder::Big);
  assert(numbers.read_integer<std::uint32_t>(ByteOrder::Big) == 0x01020304);
  must_throw([&] { numbers.read(1); });
  MemoryBackend backend;
  IO io(backend);
  BinaryStream(binary).save(io);
  assert(backend.data == binary);
  io.seek(0);
  assert(BinaryStream::load(io).bytes == binary && io.tell() == 4);
  io.seek(0);
  std::array<std::byte, 4> read;
  io.read_exact(read);
  assert(std::equal(read.begin(), read.end(), binary.begin()));
  struct SequentialBackend {
    std::size_t read(std::span<std::byte>) { return 0; }
    std::size_t write(std::span<const std::byte> input) { return input.size(); }
  } sequential;
  IO sequential_io(sequential);
  assert(BinaryStream::load(sequential_io).eof());
  UserCodec codec;
  static_assert(SerializerBackend<UserCodec, int, BinaryStream>);
  static_assert(DeserializerBackend<UserCodec, int, BinaryStream>);
  Serializer serializer(codec); Deserializer deserializer(codec);
  BinaryStream encoded;
  serializer.serialize(42, encoded);
  assert(deserializer.deserialize<int>(encoded) == 42);
  auto schema = ast_schema<"add">(ast_schema<"num">(), ast_schema<"num">());
  int sum = 0, additions = 0;
  auto visitor = compile_visitor(schema,
    on<"add">([&](const ASTNode&) { ++additions; }),
    on<"num">([&](auto tag, const ASTNode& n) { static_assert(tag.value == "num"); sum += std::stoi(n.value()); }));
  auto tree = node<"add">(leaf<"num">(2), leaf<"num">(3));
  visitor.walk(tree, VisitOrder::Postorder);
  assert(sum == 5 && additions == 1);
  auto runtime = derive_visitor(tree, on<"add">([](auto&) {}), on<"num">([](auto&) {}));
  runtime.walk(tree);
  auto compiled_runtime = compile_visitor(tree, on<"add">([](auto&) {}), on<"num">([](auto&) {}));
  compiled_runtime.walk(tree);
  must_throw([&] { derive_visitor(tree, on<"num">([](auto&) {})); });
}

void grammars_and_preprocessing() {
  auto grammar = sexpr_grammar(R"((grammar (start Expr)
    (rule Expr (Expr "+" Term) (Term)) (rule Term ("n"))))");
  auto parsed = grammar.parse({"n", "+", "n", "+", "n"});
  assert(parsed.ok() && parsed.tree->tag() == "Expr");
  auto failure = grammar.parse({"n", "+"});
  assert(!failure.ok() && failure.position == 2 && failure.expected == std::vector<std::string>{"n"});
  auto nullable = sexpr_grammar(R"((grammar (start S) (rule S (A A)) (rule A () ("a"))))");
  assert(nullable.parse({}).ok() && nullable.parse({"a"}).ok() && nullable.parse({"a", "a"}).ok());
  assert(!nullable.parse({"a", "a", "a"}).ok());
  auto typed = make_cfg<"S">(CFGProduction<"S", CFGTerminal<"a">>{});
  assert(typed.parse({"a"}).ok());
  must_throw([] { sexpr_grammar("(grammar (start A) (rule A (Missing)))"); });
  Preprocessor pp;
  pp.include = [](std::string_view path) { assert(path == "rules"); return "#define ZERO 0\n"; };
  pp.script = [](std::string_view language, std::string_view code) {
    assert(language == "lua" && code == "emit_rule"); return "(= (add ?x ZERO) ?x)";
  };
  auto source = pp.process("#include \"rules\"\n#define ENABLED 1\n#define ID(x) x\n"
    "#if defined(ENABLED) && ENABLED == 1\nID((= (mul ?x 1) ?x))\n#else\n#error inactive\n#endif\n#lua emit_rule\n");
  assert(sexpr_rules(source).size() == 2);
  assert(pp.expand("\"ZERO\" ZERO /* ZERO */") == "\"ZERO\" 0 /* ZERO */");
  pp.define("SELF", "SELF"); assert(pp.expand("SELF") == "SELF");
  assert(pp.process("#if 0\nwrong\n#elif 1\nright\n#else\nwrong\n#endif\n") == "right\n");
  assert(pp.process("#if 0\n/*\n#else\n*/\n#else\nright\n#endif\n") == "right\n");
  assert(pp.process("#define X a\\\nb\nX\n") == "ab\n");
  assert(pp.process("#if 08 == 8 && 0x10 == 16\nright\n#endif\n") == "right\n");
  must_throw([&] { pp.process("#if 1\n"); });
  must_throw([&] { pp.process("#else\n"); });
  must_throw([&] { pp.expand("ID(a,b)"); });
}

int main() {
  expressions_and_egraphs();
  binary_io_and_visitors();
  grammars_and_preprocessing();
}
