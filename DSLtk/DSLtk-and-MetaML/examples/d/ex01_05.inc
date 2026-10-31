auto s = new MetaML.Session();
auto r = s.type_of("42");
assert(r.is_ok());
assert(r.unwrap() == "Int");
