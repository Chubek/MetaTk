metaml::Session s;
auto r = s.eval("1.5 + 2.25");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "3.75");
