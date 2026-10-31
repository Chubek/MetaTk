auto s = new MetaML.Session();
auto r = s.eval("(fn x -> x + 1) 41");
assert(r.is_ok());
assert(MetaML.to_string(r.unwrap()) == "42");
