#include "MetaML.hpp"
#include <cassert>
#include <iostream>
#include <cstdint>
int main() {
metaml::Session s;
auto r = s.eval("val local_only := 42\nlocal_only");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "42");
}
