auto s = new MetaML.Session();
auto r = s.eval("type Answer := | Missing | Found Int\nmatch Found 42 with | Found n -> n | Missing -> 0");
assert(r.is_ok());
assert(MetaML.to_string(r.unwrap()) == "42");
