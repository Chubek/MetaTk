auto s = new MetaML.Session();
auto r = s.eval("val id := fn x -> x\n(id 42, id true)");
assert(r.is_ok());
assert(MetaML.to_string(r.unwrap()) == "(42, true)");
