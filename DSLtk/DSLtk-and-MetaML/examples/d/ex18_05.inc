auto s = new MetaML.Session();
auto r = s.eval("module N : sig\nval answer : Int\nend := struct\nval answer := 42\nend\nN.answer");
assert(r.is_ok());
assert(MetaML.to_string(r.unwrap()) == "42");
