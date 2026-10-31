#include "MetaML.hpp"
#include <cassert>
#include <iostream>
#include <cstdint>
int main() {
auto ab = dsl::ch('a') & dsl::ch('b');
auto ac = dsl::ch('a') & dsl::ch('c');
auto p = dsl::try_parse(ab) | ac;
assert(dsl::run_parser(p, "ac").value.has_value());
}
