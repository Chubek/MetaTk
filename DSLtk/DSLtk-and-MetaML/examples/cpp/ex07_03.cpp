#include "MetaML.hpp"
#include <cassert>
#include <iostream>
#include <cstdint>
int main() {
auto xs = dsl::parse_sexpressions("(a 1) (b 2)");
assert(xs.size() == 2);
assert(xs[1].items[0].text == "b");
}
