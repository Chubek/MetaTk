auto s = new MetaML.Session();
s.register_primitive("Host.add",
  (long a, long b) => a + b);
auto r = s.eval("Host.add 19 23");
assert(r.is_ok());
assert(MetaML.to_string(r.unwrap()) == "42");
