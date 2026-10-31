auto s = new MetaML.Session();
auto r = s.eval("module Scale := struct\nval apply := fn n -> n * 6\nend\nScale.apply 7");
assert(r.is_ok());
assert(MetaML.to_string(r.unwrap()) == "42");
