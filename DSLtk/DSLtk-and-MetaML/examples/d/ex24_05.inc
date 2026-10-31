auto active = new MetaML.Session();
assert(active.eval("val allow := fn age -> age >= 18").is_ok());
auto candidate = new MetaML.Session();
assert(candidate.eval("val allow := fn age -> age + true").is_err());
auto r = active.eval("allow 20");
assert(r.is_ok());
assert(MetaML.to_string(r.unwrap()) == "true");
