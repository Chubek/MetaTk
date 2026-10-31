#include "MetaML.hpp"
#include <cassert>
#include <iostream>
#include <cstdint>
int main() {
metaml::Session s;
auto r = s.eval("module Scale := struct\nval apply := fn n -> n * 6\nend\nScale.apply 7");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "42");
}
