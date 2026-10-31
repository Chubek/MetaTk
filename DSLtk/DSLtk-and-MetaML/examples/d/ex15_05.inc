auto s = new MetaML.Session();
auto r = s.eval("if 6 < 7 then 42 else 0");
assert(r.is_ok());
assert(MetaML.to_string(r.unwrap()) == "42");
