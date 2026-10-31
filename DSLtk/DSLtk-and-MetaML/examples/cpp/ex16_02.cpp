#include "MetaML.hpp"
#include <cassert>
#include <iostream>
#include <cstdint>
int main() {
metaml::Session s;
auto r = s.eval("1 + true");
assert(r.is_err());
assert(!r.unwrap_err().empty());
}
