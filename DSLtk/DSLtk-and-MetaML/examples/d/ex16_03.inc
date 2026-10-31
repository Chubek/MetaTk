auto s = new MetaML.Session();
auto r = s.eval("val r := {width = 6, height = 7}\nr.width * r.height");
assert(r.is_ok());
assert(MetaML.to_string(r.unwrap()) == "42");
