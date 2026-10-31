auto s = new MetaML.Session();
auto r = s.eval("module N := struct\nval answer := 42\nend\nopen N\nanswer");
assert(r.is_ok());
assert(MetaML.to_string(r.unwrap()) == "42");
