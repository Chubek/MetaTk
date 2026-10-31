auto s = new MetaML.Session();
auto r = s.eval("val make := fn n -> fn x -> n * x\nval six := make 6\nsix 7");
assert(r.is_ok());
assert(MetaML.to_string(r.unwrap()) == "42");
