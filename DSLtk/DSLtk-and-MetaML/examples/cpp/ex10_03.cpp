#include "MetaML.hpp"
#include <cassert>
#include <iostream>
#include <cstdint>
int main() {
dsl::BinaryStream s;
s.write_integer<std::uint32_t>(7);
auto bytes = s.read(4);
assert(bytes.size() == 4);
assert(s.eof());
}
