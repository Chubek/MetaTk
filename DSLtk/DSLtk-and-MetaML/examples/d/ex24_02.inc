auto s = new MetaML.Session();
s.register_primitive("Request.age", () => cast(long) 20);
auto r = s.eval("Request.age () >= 18");
assert(r.is_ok());
assert(MetaML.to_string(r.unwrap()) == "true");
