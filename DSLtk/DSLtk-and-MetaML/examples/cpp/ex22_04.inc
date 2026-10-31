metaml::Session s;
auto names = s.primitive_names();
assert(!names.empty());
assert(s.find_primitive("Int.parse") != nullptr);
