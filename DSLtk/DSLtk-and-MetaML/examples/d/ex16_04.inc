auto s = new MetaML.Session();
auto r = s.eval("val width := fn r -> r.width\nwidth {width = 42, label = \"panel\"}");
assert(r.is_ok());
assert(MetaML.to_string(r.unwrap()) == "42");
