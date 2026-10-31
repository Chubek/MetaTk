auto s = new MetaML.Session();
assert(s.eval("val size := 42").is_ok());
auto r = s.type_of("size");
assert(r.is_ok() && r.unwrap() == "Int");
