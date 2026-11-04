#include "MetaML.hpp"
#include <cassert>
#include <iostream>
#include <cstdint>
int main() {
metaml::Session s;
auto r = s.eval("missing", "policy.mml");
assert(r.is_err());
auto text = s.render(r.unwrap_err().front());
assert(!text.empty());
}
