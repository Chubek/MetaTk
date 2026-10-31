#include "MetaML.hpp"
#include <cassert>
#include <iostream>
#include <cstdint>
int main() {
metaml::Session s;
auto r = s.eval("match [42, 7] with | h :: t -> h | [] -> 0");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "42");
}
