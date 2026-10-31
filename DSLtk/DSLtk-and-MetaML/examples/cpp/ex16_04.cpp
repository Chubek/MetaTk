#include "MetaML.hpp"
#include <cassert>
#include <iostream>
#include <cstdint>
int main() {
metaml::Session s;
auto r = s.eval("val width := fn r -> r.width\nwidth {width = 42, label = \"panel\"}");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "42");
}
