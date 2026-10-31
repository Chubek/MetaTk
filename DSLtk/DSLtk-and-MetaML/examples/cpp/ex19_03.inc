metaml::Session s;
s.register_primitive("Host.allowed",
  [](std::int64_t n) { return n >= 18; });
auto r = s.eval("Host.allowed 20");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "true");
