auto s = new MetaML.Session();
auto r = s.eval("val square := fn x -> x * x\nsquare 6 + 6");
assert(r.is_ok());
assert(MetaML.to_string(r.unwrap()) == "42");
