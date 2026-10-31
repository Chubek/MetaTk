metaml::Session s;
s.register_primitive("Host.answer", []() -> std::int64_t { return 42; });
auto r = s.eval("Host.answer ()");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "42");
