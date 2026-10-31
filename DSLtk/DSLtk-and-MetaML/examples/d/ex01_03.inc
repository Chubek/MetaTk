auto s = new MetaML.Session();
auto r = s.eval("40 + 2");
assert(r.is_ok());
assert(MetaML.to_string(r.unwrap()) == "42");
