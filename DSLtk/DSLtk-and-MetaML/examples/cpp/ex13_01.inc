metaml::Session s;
auto r = s.eval("42");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "42");
