#include "MetaML.hpp"
#include <cassert>
#include <iostream>
#include <cstdint>
int main() {
metaml::Session s;
s.add_search_path("modules");
auto paths = s.search_paths();
assert(!paths.empty());
assert(paths.front() == "modules");
}
