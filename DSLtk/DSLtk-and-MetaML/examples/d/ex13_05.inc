auto s = new MetaML.Session();
auto r = s.eval("val local_only := 42\nlocal_only");
assert(r.is_ok());
assert(MetaML.to_string(r.unwrap()) == "42");
