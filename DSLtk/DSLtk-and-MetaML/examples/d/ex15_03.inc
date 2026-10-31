auto s = new MetaML.Session();
auto r = s.eval("let x = 6 in x * 7");
assert(r.is_ok());
assert(MetaML.to_string(r.unwrap()) == "42");
