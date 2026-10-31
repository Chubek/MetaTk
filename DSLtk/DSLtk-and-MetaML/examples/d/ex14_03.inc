auto s = new MetaML.Session();
auto r = s.eval("String.length \"hello\"");
assert(r.is_ok());
assert(MetaML.to_string(r.unwrap()) == "5");
