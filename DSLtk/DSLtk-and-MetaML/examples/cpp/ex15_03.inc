metaml::Session s;
auto r = s.eval("let x = 6 in x * 7");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "42");
