auto s = new MetaML.Session();
auto r = s.eval("module Outer := struct\nmodule Inner := struct\nval answer := 42\nend\nend\nOuter.Inner.answer");
assert(r.is_ok());
assert(MetaML.to_string(r.unwrap()) == "42");
