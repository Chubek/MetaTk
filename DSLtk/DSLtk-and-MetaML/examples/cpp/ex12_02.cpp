#include "MetaML.hpp"
#include <cassert>
#include <iostream>
#include <cstdint>
int main() {
auto units = dsl::literal_set(
  dsl::lit<"_km">([](long double x) { return x * 1000; }));
assert(units.parse("2_km") == 2000);
}
