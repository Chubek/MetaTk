#include "MetaML.hpp"
#include <cassert>
#include <iostream>
#include <cstdint>
int main() {
metaml::Session s;
auto r = s.load_stdlib("__book_missing_metalib__");
assert(r.is_err());
assert(!s.stdlib_loaded());
}
