#include "MetaML.hpp"
#include <cassert>
#include <iostream>
#include <cstdint>
int main() {
metaml::Session s;
auto r = s.eval("40 + 2");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "42");
}
