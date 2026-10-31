metaml::Session s;
auto r = s.eval("(fn x -> x + 1) 41");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "42");
