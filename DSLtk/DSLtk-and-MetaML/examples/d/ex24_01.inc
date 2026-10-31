auto s = new MetaML.Session();
auto r = s.eval("module Policy := struct\nval allow := fn age -> age >= 18\nend\nPolicy.allow 20");
assert(r.is_ok());
assert(MetaML.to_string(r.unwrap()) == "true");
