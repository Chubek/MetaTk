metaml::Session s;
auto r = s.eval("[1, 2, 3]");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "[1, 2, 3]");
