#include "MetaML.hpp"
#include <cassert>
#include <iostream>
#include <cstdint>
int main() {
metaml::Session s;
auto r = s.type_of("42");
assert(r.is_ok());
assert(r.unwrap() == "Int");
}
