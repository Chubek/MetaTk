#include "MetaML.hpp"
#include <cassert>
#include <iostream>
#include <cstdint>
int main() {
assert(dsl::pattern<"[0-9]+">::matches("2048"));
assert(!dsl::pattern<"[0-9]+">::matches("20x"));
}
