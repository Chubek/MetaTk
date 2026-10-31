#include "MetaML.hpp"
#include <cassert>
#include <iostream>
#include <cstdint>
int main() {
metaml::Session s;
auto r = s.eval("type Answer := | Missing | Found Int\nmatch Found 42 with | Found n -> n | Missing -> 0");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "42");
}
