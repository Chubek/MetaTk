#include "DSLtk.hpp"
int main() {
#if defined(PROBE_PREDICATE)
  auto a = dsl::predicate([](int x) { return x > 0; });
  auto b = dsl::predicate([](int x) { return x < 10; });
  return (a & b)(5) ? 0 : 1;
#elif defined(PROBE_RESULT)
  auto result = dsl::Result<std::string, std::string>::from_ok("ok");
  return result.is_ok() ? 0 : 1;
#elif defined(PROBE_TASK)
  dsl::Task task{"task", [](dsl::TaskState&) {
    return dsl::Result<std::string, std::string>::from_ok("ok");
  }};
  dsl::TaskState state;
  auto chain = task | task;
  dsl::run(state, chain);
#elif defined(PROBE_PATTERN)
  auto table = dsl::match(
    dsl::when<dsl::pattern<"[0-9]+">{}>([](std::string_view) { return 1; }),
    dsl::otherwise([](std::string_view) { return 0; }));
  auto nfa = dsl::match(
    dsl::when<dsl::StaticRegex<"(if|while)">{}>([](std::string_view) { return 1; }),
    dsl::otherwise([](std::string_view) { return 0; }));
  auto a = dsl::leaf<"number">(1), b = dsl::leaf<"number">(2);
  return table(std::string_view("123")) == 1
      && nfa(std::string_view("while")) == 1 && a != b ? 0 : 1;
#endif
}
