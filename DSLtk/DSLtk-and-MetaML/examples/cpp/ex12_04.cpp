#include "MetaML.hpp"
#include <cassert>
#include <iostream>
#include <cstdint>
int main() {
auto schema = dsl::leaf<"n">(0);
auto v = dsl::derive_visitor(schema,
  dsl::on<"n">([](const dsl::ASTNode&) {}));
auto n = dsl::leaf<"n">(8);
v.visit(n);
assert(n.value() == "8");
}
