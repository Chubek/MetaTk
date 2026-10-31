#include "MetaML.hpp"
#include <cassert>
#include <iostream>
#include <cstdint>
int main() {
metaml::Session s;
auto path = s.resolve_import("__book_missing_module__");
assert(!path.has_value());
}
