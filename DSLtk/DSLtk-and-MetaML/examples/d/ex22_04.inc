auto s = new MetaML.Session();
auto names = s.primitive_names();
assert(names.length > 0);
assert(s.find_primitive("Int.parse") !is null);
