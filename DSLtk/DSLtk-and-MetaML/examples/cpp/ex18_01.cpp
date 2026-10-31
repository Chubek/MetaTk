#include "MetaML.hpp"
#include <cassert>
#include <iostream>
#include <cstdint>
int main() {
metaml::Session s;
auto r = s.eval("module Numbers := struct\nval answer := 42\nend\nNumbers.answer");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "42");
}
