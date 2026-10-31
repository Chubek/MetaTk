#include "MetaML.hpp"
#include <cassert>
#include <iostream>
#include <cstdint>
int main() {
metaml::Session s;
auto r = s.eval("(1, true, \"ok\")");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "(1, true, \"ok\")");
}
