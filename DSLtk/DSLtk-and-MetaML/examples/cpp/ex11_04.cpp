#include "MetaML.hpp"
#include <cassert>
#include <iostream>
#include <cstdint>
int main() {
std::string source = "one\ntwo";
dsl::SourceMap map(source);
auto pos = map.line_column(4);
assert(pos.first == 2 && pos.second == 1);
assert(map.line_text(2) == "two");
}
