auto s = new MetaML.Session();
auto r = s.eval("(1, true, \"ok\")");
assert(r.is_ok());
assert(MetaML.to_string(r.unwrap()) == "(1, true, \"ok\")");
