auto s = new MetaML.Session();
auto r = s.eval("val add := fn x y -> x + y\nval add40 := add 40\nadd40 2");
assert(r.is_ok());
assert(MetaML.to_string(r.unwrap()) == "42");
