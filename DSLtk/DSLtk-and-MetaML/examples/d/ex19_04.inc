auto s = new MetaML.Session();
s.register_primitive("Host.label",
  (string text) => text ~ "!");
auto r = s.eval("Host.label \"ok\"");
assert(r.is_ok());
assert(MetaML.to_string(r.unwrap()) == "\"ok!\"");
