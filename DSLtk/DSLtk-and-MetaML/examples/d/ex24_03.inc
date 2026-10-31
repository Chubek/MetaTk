auto candidate = new MetaML.Session();
assert(candidate.eval("val allow := fn age -> age >= 18").is_ok());
auto type = candidate.type_of("allow 20");
assert(type.is_ok() && type.unwrap() == "Bool");
auto r = candidate.eval("allow 17");
assert(r.is_ok());
assert(MetaML.to_string(r.unwrap()) == "false");
