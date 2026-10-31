auto s = new MetaML.Session();
auto r = s.eval("match Some 42 with | Some n when n > 0 -> n | _ -> 0");
assert(r.is_ok());
assert(MetaML.to_string(r.unwrap()) == "42");
