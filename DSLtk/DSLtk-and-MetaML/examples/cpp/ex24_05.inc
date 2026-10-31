metaml::Session active;
assert(active.eval("val allow := fn age -> age >= 18").is_ok());
metaml::Session candidate;
assert(candidate.eval("val allow := fn age -> age + true").is_err());
auto r = active.eval("allow 20");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "true");
