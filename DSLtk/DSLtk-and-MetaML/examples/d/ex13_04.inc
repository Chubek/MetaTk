auto s = new MetaML.Session();
auto r = s.eval("val default_limit := 42\ndefault_limit");
assert(r.is_ok());
assert(MetaML.to_string(r.unwrap()) == "42");
