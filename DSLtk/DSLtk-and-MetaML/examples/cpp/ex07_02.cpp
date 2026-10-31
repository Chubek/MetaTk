#include "MetaML.hpp"
#include <cassert>
#include <iostream>
#include <cstdint>
int main() {
auto s = dsl::SExpr::atom("two words");
auto text = s.dump();
auto back = dsl::parse_sexpr(text);
assert(back.text == "two words");
assert(back.quoted);
}
