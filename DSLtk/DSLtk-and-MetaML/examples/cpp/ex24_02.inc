metaml::Session s;
s.register_primitive("Request.age", []() -> std::int64_t { return 20; });
auto r = s.eval("Request.age () >= 18");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "true");
