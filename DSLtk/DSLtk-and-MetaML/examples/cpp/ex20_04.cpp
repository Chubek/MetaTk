#include "MetaML.hpp"
#include <cassert>
#include <iostream>
#include <cstdint>
int main() {
metaml::Session s;
auto r = s.type_of("fn x -> x");
assert(r.is_ok());
assert(r.unwrap().find("->") != std::string::npos);
}
