#include "MetaML.hpp"
#include <cassert>
#include <iostream>
#include <cstdint>
int main() {
auto positive = dsl::predicate([](int x) { return x > 0; });
auto even = dsl::predicate([](int x) { return x % 2 == 0; });
auto valid = positive & even;
assert(valid(8));
assert(!valid(-2));
}
