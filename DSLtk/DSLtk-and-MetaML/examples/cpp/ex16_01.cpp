#include "MetaML.hpp"
#include <cassert>
#include <iostream>
#include <cstdint>
int main() {
metaml::Session s;
auto r = s.eval("val id := fn x -> x\n(id 42, id true)");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "(42, true)");
}
