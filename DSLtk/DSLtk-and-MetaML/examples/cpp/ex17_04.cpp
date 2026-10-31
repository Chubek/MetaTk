#include "MetaML.hpp"
#include <cassert>
#include <iostream>
#include <cstdint>
int main() {
metaml::Session s;
auto r = s.eval("match Some 42 with | Some n when n > 0 -> n | _ -> 0");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "42");
}
