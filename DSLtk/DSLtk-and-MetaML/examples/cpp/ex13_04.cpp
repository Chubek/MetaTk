#include "MetaML.hpp"
#include <cassert>
#include <iostream>
#include <cstdint>
int main() {
metaml::Session s;
auto r = s.eval("val default_limit := 42\ndefault_limit");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "42");
}
