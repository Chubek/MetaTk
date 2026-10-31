#include "MetaML.hpp"
#include <cassert>
#include <iostream>
#include <cstdint>
int main() {
metaml::Session s;
auto r = s.eval("module Outer := struct\nmodule Inner := struct\nval answer := 42\nend\nend\nOuter.Inner.answer");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "42");
}
