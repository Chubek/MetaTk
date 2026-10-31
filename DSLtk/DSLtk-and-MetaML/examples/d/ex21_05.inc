auto s = new MetaML.Session();
s.set_max_depth(1000);
auto r = s.eval("40 + 2");
assert(r.is_ok());
assert(MetaML.to_string(r.unwrap()) == "42");
