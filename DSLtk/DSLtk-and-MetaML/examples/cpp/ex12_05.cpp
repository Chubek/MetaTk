#include "MetaML.hpp"
#include <cassert>
#include <iostream>
#include <cstdint>
int main() {
auto metres = [](std::string_view text) {
  auto ls = dsl::literal_set(dsl::lit<"_m">(
    [](long double n) { return n; }));
  return ls.parse(text);
};
assert(metres("12_m") == 12);
}
