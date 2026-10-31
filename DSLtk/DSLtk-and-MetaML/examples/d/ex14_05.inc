auto s = new MetaML.Session();
auto r = s.eval("[1, 2, 3]");
assert(r.is_ok());
assert(MetaML.to_string(r.unwrap()) == "[1, 2, 3]");
