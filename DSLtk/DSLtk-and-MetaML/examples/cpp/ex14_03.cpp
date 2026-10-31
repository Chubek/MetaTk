#include "MetaML.hpp"
#include <cassert>
#include <iostream>
#include <cstdint>
int main() {
metaml::Session s;
auto r = s.eval("String.length \"hello\"");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "5");
}
