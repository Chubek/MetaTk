auto s = new MetaML.Session();
auto r = s.eval("module Numbers := struct\nval answer := 42\nend\nNumbers.answer");
assert(r.is_ok());
assert(MetaML.to_string(r.unwrap()) == "42");
