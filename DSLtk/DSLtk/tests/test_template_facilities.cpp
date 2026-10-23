#include "DSLtk.hpp"
#include <cassert>
#include <memory>
#include <type_traits>

template<class F> void must_throw(F fn) {
  bool thrown = false;
  try { fn(); } catch (const std::runtime_error&) { thrown = true; }
  assert(thrown);
}

void match_tables() {
  const auto mixed = dsl::match(
      dsl::when<1>([](int) { return 10; }),
      dsl::when<2>([](int) { return 2.5; }),
      dsl::otherwise([](int) { return 3L; }));
  static_assert(std::same_as<decltype(mixed(1)), double>);
  assert(mixed(1) == 10 && mixed(2) == 2.5 && mixed(3) == 3);

  const auto patterns = dsl::match(
      dsl::when<dsl::pattern<"[0-9]+">>([](std::string_view) { return 1; }),
      dsl::when<dsl::pattern<"[a-z]+">{}>([](std::string_view) { return 2; }),
      dsl::otherwise([](std::string_view) { return 0; }));
  assert(patterns("123") == 1 && patterns("abc") == 2 && patterns("?") == 0);

  int calls = 0;
  const auto actions = dsl::match(
      dsl::when<1>([&](int, int& value) { ++calls; value += 1; }),
      dsl::when<1>([&](int, int&) { calls += 100; }),
      dsl::otherwise([&](int, int& value) { ++calls; value += 2; }));
  static_assert(std::is_void_v<decltype(actions(1, calls))>);
  int value = 0;
  actions(1, value);
  assert(calls == 1 && value == 1);
  actions(0, value);
  assert(calls == 2 && value == 3);
  const auto no_fallback = dsl::match(dsl::when<1>([](int) {}));
  no_fallback(1);
  must_throw([&] { no_fallback(0); });
  must_throw([&] { dsl::match(dsl::when<1>([](int) { return 2; }))(0); });

  const auto owning = dsl::match(
      dsl::when<1>([](int, std::unique_ptr<int> p) { return p; }),
      dsl::otherwise([](int, std::unique_ptr<int> p) { ++*p; return p; }));
  assert(*owning(1, std::make_unique<int>(4)) == 4);
  assert(*owning(0, std::make_unique<int>(4)) == 5);

  struct ConstructOnly {
    int value;
    explicit ConstructOnly(int n) : value(n) {}
    ConstructOnly(ConstructOnly&&) = default;
    ConstructOnly& operator=(ConstructOnly&&) = delete;
  };
  auto construct_only = dsl::match(
      dsl::when<1>([](int) { return ConstructOnly(7); }),
      dsl::otherwise([](int) { return ConstructOnly(8); }));
  assert(construct_only(1).value == 7 && construct_only(0).value == 8);
}

struct RefOverload {
  int operator()(const int& value) const { return value + 1; }
  double operator()(int&&) const { return -1.0; }
};

struct MaybeOverload {
  dsl::Maybe<int> operator()(const int& value) const { return value + 1; }
  dsl::Maybe<double> operator()(int&&) const { return -1.0; }
};

struct ResultOverload {
  dsl::Result<int, int> operator()(const int& value) const { return value + 1; }
  dsl::Result<double, int> operator()(int&&) const { return -1.0; }
};

void monadic_results() {
  using R = dsl::Result<int, int>;
  const auto ok = R::from_ok(4);
  const auto err = R::from_err(9);
  assert(ok.is_ok() && !ok.is_err() && ok.unwrap() == 4);
  assert(err.is_err() && !err.is_ok() && err.unwrap_or(7) == 7);
  must_throw([&] { err.unwrap(); });
  auto mapped = ok.map(RefOverload{});
  static_assert(std::same_as<decltype(mapped), R>);
  assert(mapped.unwrap() == 5);
  auto recovered = err.map_err(RefOverload{});
  static_assert(std::same_as<decltype(recovered), R>);
  assert(recovered.is_err());
  int observed_error = 0;
  recovered.map_err([&](const int& e) { observed_error = e; return e; });
  assert(observed_error == 10);
  assert(ok.map_err(RefOverload{}).unwrap() == 4);
  auto chained = ok.and_then(ResultOverload{});
  static_assert(std::same_as<decltype(chained), R>);
  assert(chained.unwrap() == 5);
  assert(err.and_then(ResultOverload{}).is_err());
  int calls = 0;
  assert(err.map([&](int v) { ++calls; return v; }).is_err());
  assert(calls == 0);

  const dsl::Maybe<int> present(4), empty;
  auto maybe = present.map(RefOverload{});
  static_assert(std::same_as<decltype(maybe), dsl::Maybe<int>>);
  assert(maybe.or_else(0) == 5);
  assert(!empty.map(RefOverload{}).has_value());
  auto flat = present.flat_map(MaybeOverload{});
  static_assert(std::same_as<decltype(flat), dsl::Maybe<int>>);
  assert(flat.or_else(0) == 5);
  assert(!empty.flat_map(MaybeOverload{}).has_value());

  auto identity = [](const int& v) -> const int& { return v; };
  assert(present.map(identity).or_else(0) == 4);
  assert(ok.map(identity).unwrap() == 4);
  assert(err.map_err(identity).is_err());
  std::optional<int> opt = 3;
  auto copied = dsl::map(opt, identity);
  static_assert(std::same_as<decltype(copied), std::optional<int>>);
  opt = 8;
  assert(*copied == 3);
  assert(!dsl::map(std::optional<int>{}, identity));
}

struct Scalar : dsl::DSL<Scalar, dsl::ExprTemplates> {
  int value;
  constexpr explicit Scalar(int v) : value(v) {}
  constexpr int expr_value() const { return value; }
};
constexpr Scalar a(6), b(2);
static_assert((-(a + b)).eval() == -8);
static_assert((-(-a)).eval() == 6);
static_assert(((-a) + b).eval() == -4);
static_assert(((-a) - b).eval() == -8);
static_assert(((-a) * b).eval() == -12);
static_assert(((-a) / b).eval() == -3);
static_assert(((-a) + (-b) * (a - b)).eval() == -14);

void tasks() {
  using R = dsl::Result<std::string, std::string>;
  dsl::TaskState state;
  int calls = 0;
  dsl::Task task{"event", [&](dsl::TaskState&) { ++calls; return R::from_ok("done"); }};
  dsl::upon(state, [] { return false; }, task);
  assert(calls == 0 && state.results.empty());
  dsl::upon(state, [] { return true; }, task);
  assert(calls == 1 && state.results.at("event").unwrap() == "done");
  dsl::upon(state, [] { return true; }, {"event", [](auto&) { return R::from_err("failed"); }});
  assert(state.results.at("event").is_err());
  dsl::Task fail{"fail", [](auto&) { return R::from_err("failed"); }};
  dsl::Task after{"after", [&](auto&) { ++calls; return R::from_ok("after"); }};
  auto chain = fail | after;
  dsl::run(state, chain);
  assert(calls == 1 && !state.results.contains("after"));
  chain.policy = dsl::TaskPolicy::ContinueOnError;
  dsl::run(state, chain);
  assert(calls == 2 && state.results.at("after").is_ok());
}

void parsers_and_graphs() {
  auto owning = dsl::parser([](dsl::ParsecInput& in) -> dsl::ExpectedResult<std::unique_ptr<char>> {
    if (in.eof()) return std::nullopt;
    return std::make_unique<char>(in.consume());
  });
  auto repeated = dsl::run_parser(*owning, "ab");
  assert(repeated.value && repeated.value->size() == 2);
  assert(*repeated.value->at(0) == 'a' && *repeated.value->at(1) == 'b');
  auto pair = dsl::run_parser(owning & owning, "ab");
  assert(pair.value && *pair.value->first == 'a' && *pair.value->second == 'b');
  auto optional = dsl::run_parser(dsl::optional(owning), "a");
  assert(optional.value && **optional.value && ***optional.value == 'a');
  auto absent = dsl::run_parser(dsl::optional(owning), "");
  assert(absent.value && !*absent.value);

  // Empty successes must terminate repetition rather than loop forever.
  auto stalled = dsl::run_parser(*dsl::optional(dsl::ch('a')), "a");
  assert(!stalled.value && stalled.error.kind == dsl::ParseFailureKind::Committed);
  assert(stalled.error.pos == 1);
  assert(!dsl::run_parser(dsl::ch('\0'), "").value);
  assert(dsl::run_parser(dsl::ch('\0'), std::string_view("\0", 1)).value);

  dsl::DirectedGraph<int, std::unique_ptr<int>> graph;
  auto x = graph.add_node(1), y = graph.add_node(2);
  auto edge = graph.add_edge(x, y, std::make_unique<int>(3));
  graph.remove_edge(edge);
  assert(graph.outgoing(x).empty() && graph.incoming(y).empty());
  graph.add_edge(x, x, std::make_unique<int>(4));
  graph.add_edge(x, y, std::make_unique<int>(5));
  graph.remove_node(x);
  assert(graph.edge_count() == 0 && graph.node_count() == 1);
}

int main() {
  match_tables();
  monadic_results();
  tasks();
  parsers_and_graphs();
}
