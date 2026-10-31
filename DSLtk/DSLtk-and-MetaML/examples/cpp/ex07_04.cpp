#include "MetaML.hpp"
#include <cassert>
#include <iostream>
#include <cstdint>
int main() {
auto n = dsl::leaf<"name">("two words");
auto sexpr = dsl::ast_to_sexpr(n);
auto back = dsl::sexpr_to_ast(sexpr);
assert(back == n);
}
