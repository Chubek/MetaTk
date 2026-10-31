auto s = new MetaML.Session();
auto r = s.eval("2 + 5 * 8");
assert(r.is_ok());
assert(MetaML.to_string(r.unwrap()) == "42");
