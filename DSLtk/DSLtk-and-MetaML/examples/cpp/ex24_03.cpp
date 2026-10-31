#include "MetaML.hpp"
#include <cassert>
#include <iostream>
#include <cstdint>
int main() {
metaml::Session candidate;
assert(candidate.eval("val allow := fn age -> age >= 18").is_ok());
auto type = candidate.type_of("allow 20");
assert(type.is_ok() && type.unwrap() == "Bool");
auto r = candidate.eval("allow 17");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "false");
}
