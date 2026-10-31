#include "MetaML.hpp"
#include <cassert>
#include <iostream>
#include <cstdint>
int main() {
metaml::Session s;
auto r = s.eval("unbound_name + 1");
assert(r.is_err());
assert(!r.unwrap_err().empty());
}
