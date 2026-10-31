auto s = new MetaML.Session();
auto r = s.eval("1.5 + 2.25");
assert(r.is_ok());
assert(MetaML.to_string(r.unwrap()) == "3.75");
