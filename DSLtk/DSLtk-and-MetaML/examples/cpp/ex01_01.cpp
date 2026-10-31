#include "MetaML.hpp"
#include <cassert>
#include <iostream>
#include <cstdint>
int main() {
struct Domain : dsl::DSL<Domain, dsl::Pipeline> {};
Domain domain;
auto stage = dsl::pipe([](int x) { return x + 1; });
assert((domain.wrap(41) | stage) == 42);
}
