auto s = new MetaML.Session();
auto r = s.eval("match Ok 42 with | Ok x -> x | Err e -> 0");
assert(r.is_ok());
assert(MetaML.to_string(r.unwrap()) == "42");
