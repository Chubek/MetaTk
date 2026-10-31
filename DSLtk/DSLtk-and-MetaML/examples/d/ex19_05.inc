auto s = new MetaML.Session();
s.register_primitive("Host.answer", () => cast(long) 42);
auto r = s.eval("Host.answer ()");
assert(r.is_ok());
assert(MetaML.to_string(r.unwrap()) == "42");
