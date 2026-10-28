#include "DSLtk.hpp"
#include <cmath>
#include <iostream>

namespace physics {
template<int Length, int Time> struct Quantity {
  long double value;
};
using Length = Quantity<1, 0>;
using Duration = Quantity<0, 1>;
using Speed = Quantity<1, -1>;
template<int L, int T>
Quantity<L, T> operator+(Quantity<L, T> a, Quantity<L, T> b) {
  return {a.value + b.value};
}
template<int L1, int T1, int L2, int T2>
Quantity<L1 - L2, T1 - T2> operator/(Quantity<L1, T1> a,
                                                   Quantity<L2, T2> b) {
  if (b.value == 0) throw std::domain_error("zero divisor");
  return {a.value / b.value};
}
struct Error { std::string message; };
struct Units : dsl::DSL<Units, dsl::Pipeline, dsl::CustomLiterals> {
  static inline const auto literals = dsl::literal_set(
      dsl::lit<"km">([](long double x) { return x * 1000; }),
      dsl::lit<"m">([](long double x) { return x; }));
};
dsl::Result<Length, Error> parse_length(std::string_view text) {
  // Validate the whole spelling before LiteralSet's prefix conversion.
  if (!dsl::StaticRegex<"[0-9]+(\\.[0-9]+)?(km|m)">::matches(text))
    return dsl::Result<Length, Error>::from_err({"invalid length"});
  try {
    auto value = Units::parse_literal(text);
    if (!std::isfinite(value))
      return dsl::Result<Length, Error>::from_err({"nonfinite length"});
    return dsl::Result<Length, Error>::from_ok({value});
  } catch (const std::exception&) {
    return dsl::Result<Length, Error>::from_err({"length out of range"});
  }
}
constexpr Duration operator""_s(long double x) { return {x}; }
}

int main() {
  using namespace physics;
  auto distance = parse_length("1.5km");
  if (distance.is_err()) return 1;
  auto speed = Units::wrap(distance.unwrap())
      | dsl::pipe([](Length x) { return x / 120.0_s; });
  static_assert(std::same_as<decltype(speed), Speed>);
  assert(speed.value == 12.5L);
  assert(parse_length("1junkm").is_err());
  assert(parse_length("km").is_err());
  bool zero_rejected = false;
  try { (void)(Length{1} / Duration{0}); }
  catch (const std::domain_error&) { zero_rejected = true; }
  assert(zero_rejected);
  std::cout << "1500 m / 120 s = " << speed.value << " m/s\n";
}
