#include "MetaML.hpp"
#include <cassert>
#include <iostream>
#include <cstdint>
int main() {
auto n = dsl::leaf<"n">(42);
int seen = 0;
auto v = dsl::derive_visitor(n,
  dsl::on<"n">([&](const dsl::ASTNode&) { ++seen; }));
v.walk(n);
assert(seen == 1);
}
