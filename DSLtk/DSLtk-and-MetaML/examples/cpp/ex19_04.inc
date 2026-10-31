metaml::Session s;
s.register_primitive("Host.label",
  [](std::string text) { return text + "!"; });
auto r = s.eval("Host.label \"ok\"");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "\"ok!\"");
