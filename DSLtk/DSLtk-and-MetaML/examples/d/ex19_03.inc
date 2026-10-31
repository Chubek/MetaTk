auto s = new MetaML.Session();
s.register_primitive("Host.allowed",
  (long n) => n >= 18);
auto r = s.eval("Host.allowed 20");
assert(r.is_ok());
assert(MetaML.to_string(r.unwrap()) == "true");
