#include "MetaML.hpp"
#include <cassert>
#include <iostream>
#include <cstdint>
int main() {
metaml::Session s;
int state = 0;
s.on_close([&] { state = 1; }, [&] { assert(state == 1); state = 2; });
s.close();
assert(state == 2);
}
