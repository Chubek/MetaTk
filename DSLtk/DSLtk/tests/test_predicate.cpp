#include "DSLtk.hpp"
#include <cassert>

struct PredicateDSL : dsl::DSL<PredicateDSL, dsl::Operators> {};

constexpr auto positive = dsl::predicate([](int x) { return x > 0; });
constexpr auto even = PredicateDSL::make_pred([](int x) { return x % 2 == 0; });

constexpr bool truth_table() {
  const auto both = positive & even;
  const auto either = positive | even;
  const auto negated = !positive;
  const auto nested = !(positive & even) | (positive & !even);
  for (int x = -3; x <= 3; ++x) {
    const bool p = x > 0, e = x % 2 == 0;
    if (both(x) != (p && e) || either(x) != (p || e)
        || negated(x) != !p || nested(x) != (!(p && e) || (p && !e)))
      return false;
  }
  return true;
}
static_assert(truth_table());

constexpr bool nonzero(int x) { return x != 0; }
static_assert((dsl::predicate(nonzero) & positive)(2));

int main() {
  assert(truth_table());

  int calls = 0;
  auto counted = dsl::predicate([&calls](int x) { ++calls; return x % 2 == 0; });
  auto both = positive & counted;
  auto either = positive | counted;
  assert(!both(-2) && calls == 0);
  assert(both(2) && calls == 1);
  assert(either(1) && calls == 1);
  assert(either(-2) && calls == 2);
  assert(!either(-1) && calls == 3);
  assert((!counted)(1) && calls == 4);

  auto above = PredicateDSL::make_pred([limit = 3](const int& x) { return x > limit; });
  const auto rule = above & even;
  int value = 4;
  assert(rule(value));
  value = 2;
  assert(!rule(value));
}
