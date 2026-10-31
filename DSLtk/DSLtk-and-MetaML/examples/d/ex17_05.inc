auto s = new MetaML.Session();
auto r = s.eval("match [42, 7] with | h :: t -> h | [] -> 0");
assert(r.is_ok());
assert(MetaML.to_string(r.unwrap()) == "42");
