auto s = new MetaML.Session();
auto r = s.eval("val x := 6\nval y := 7\nx * y");
assert(r.is_ok());
assert(MetaML.to_string(r.unwrap()) == "42");
