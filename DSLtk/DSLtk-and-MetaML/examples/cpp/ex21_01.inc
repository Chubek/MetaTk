metaml::Session s;
s.set_primitive_allowlist({});
auto r = s.eval("1 + 2");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "3");
