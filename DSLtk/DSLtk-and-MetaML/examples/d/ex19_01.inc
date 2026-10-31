auto s = new MetaML.Session();
s.register_primitive("Host.twice", (long n) => n * 2);
auto r = s.eval("Host.twice 21");
assert(r.is_ok());
assert(MetaML.to_string(r.unwrap()) == "42");
