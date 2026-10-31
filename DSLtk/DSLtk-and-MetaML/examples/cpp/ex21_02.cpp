#include "MetaML.hpp"
#include <cassert>
#include <iostream>
#include <cstdint>
int main() {
metaml::Session s;
std::string captured;
s.set_output([&](std::string_view x) { captured += x; });
s.write("hello");
assert(captured == "hello");
}
