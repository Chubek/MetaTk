auto s = new MetaML.Session();
assert(s.eval("val base := 40").is_ok());
auto r = s.eval("base + 2");
assert(r.is_ok());
assert(MetaML.to_string(r.unwrap()) == "42");
