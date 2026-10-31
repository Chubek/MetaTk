#include "MetaML.hpp"
#include <cassert>
#include <iostream>
#include <cstdint>
int main() {
auto twice = dsl::pipe([](int x) { return x * 2; });
auto answer = 21 | twice;
assert(answer == 42);
}
