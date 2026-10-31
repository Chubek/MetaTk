#include "MetaML.hpp"
#include <cassert>
#include <iostream>
#include <cstdint>
int main() {
enum class Mode { Read, Write };
assert(dsl::enum_name(Mode::Read) == "Read");
assert(dsl::enum_cast<Mode>("Write").value() == Mode::Write);
}
