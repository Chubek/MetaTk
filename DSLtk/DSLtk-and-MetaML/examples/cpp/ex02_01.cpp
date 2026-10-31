#include "MetaML.hpp"
#include <cassert>
#include <iostream>
#include <cstdint>
int main() {
auto add = dsl::pipe([](int x) { return x + 3; });
auto mul = dsl::pipe([](int x) { return x * 4; });
assert((2 | add | mul) == 20);
}
