#include "MetaML.hpp"
#include <cassert>
#include <iostream>
#include <cstdint>
int main() {
metaml::Session s;
auto r = s.eval("module N : sig\nval answer : Int\nend := struct\nval answer := 42\nend\nN.answer");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "42");
}
