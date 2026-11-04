/** Immutable arbitrary-precision signed integers. Requires Boost headers.
 * Decimal parsing is strict; division truncates toward zero. */
#pragma once
#include "../cpp/Support.h"
#include <boost/multiprecision/cpp_int.hpp>
namespace metalib {
namespace bignum_detail {
using Big = boost::multiprecision::cpp_int;
struct Number : metaml::ForeignObj {
  const Big value;
  explicit Number(Big n) : ForeignObj("BigInt"), value(std::move(n)) {}
};
inline support::Value wrap(Big n) {
  support::Value v; v.kind = metaml::VK::Foreign;
  v.obj = std::make_shared<Number>(std::move(n)); return v;
}
inline const Big &get(const support::Value &v, support::C &c) {
  auto p = std::dynamic_pointer_cast<Number>(v.obj);
  if (v.kind != metaml::VK::Foreign || !p) c.error("M0404", "expected a BigInt");
  return p->value;
}
}
inline void install_Bignum(metaml::Session &s) {
  using namespace support;
  using namespace bignum_detail;
  if (s.find_primitive("Bignum.parse")) return;
  s.declare("type BigInt\n", "<metalib Bignum>");
  prim(s, "Bignum.parse", "String -> Result BigInt String", 1, [](C &, V &a) {
    const auto &text = S(a[0]); std::size_t i = 0; bool negative = false;
    if (!text.empty() && (text[0] == '-' || text[0] == '+')) { negative = text[0] == '-'; ++i; }
    if (i == text.size()) return err("Bignum: expected decimal digits");
    Big n = 0;
    for (; i < text.size(); ++i) {
      if (text[i] < '0' || text[i] > '9') return err("Bignum: expected decimal digits");
      n *= 10; n += text[i] - '0';
    }
    if (negative) n = -n;
    return ok(wrap(std::move(n)));
  });
  prim(s, "Bignum.of_int", "Int -> BigInt", 1, [](C &, V &a) { return wrap(Big(a[0].i)); });
  prim(s, "Bignum.to_string", "BigInt -> String", 1, [](C &c, V &a) { return str(get(a[0], c).str()); });
  prim(s, "Bignum.to_int", "BigInt -> Option Int", 1, [](C &c, V &a) {
    const auto &n = get(a[0], c);
    if (n < std::numeric_limits<std::int64_t>::min() || n > std::numeric_limits<std::int64_t>::max()) return none();
    return some(integer(n.convert_to<std::int64_t>()));
  });
  prim(s, "Bignum.add", "BigInt -> BigInt -> BigInt", 2, [](C &c, V &a) { return wrap(get(a[0], c) + get(a[1], c)); });
  prim(s, "Bignum.sub", "BigInt -> BigInt -> BigInt", 2, [](C &c, V &a) { return wrap(get(a[0], c) - get(a[1], c)); });
  prim(s, "Bignum.mul", "BigInt -> BigInt -> BigInt", 2, [](C &c, V &a) { return wrap(get(a[0], c) * get(a[1], c)); });
  prim(s, "Bignum.neg", "BigInt -> BigInt", 1, [](C &c, V &a) { return wrap(-get(a[0], c)); });
  prim(s, "Bignum.compare", "BigInt -> BigInt -> Int", 2, [](C &c, V &a) {
    const auto &x = get(a[0], c), &y = get(a[1], c); return integer(x < y ? -1 : x > y ? 1 : 0);
  });
  prim(s, "Bignum.divmod", "BigInt -> BigInt -> Result (BigInt, BigInt) String", 2, [](C &c, V &a) {
    const auto &x = get(a[0], c), &y = get(a[1], c);
    if (y == 0) return err("Bignum: division by zero");
    return ok(pair(wrap(x / y), wrap(x % y)));
  });
}
}
