#include "MetaML.hpp"
#include <cassert>
#include <iostream>
#include <cstdint>
int main() {
metaml::Session s;
assert(s.eval("val size := 42").is_ok());
auto r = s.type_of("size");
assert(r.is_ok() && r.unwrap() == "Int");
}
