auto s = new MetaML.Session();
auto r = s.eval("unbound_name + 1");
assert(r.is_err());
assert(r.unwrap_err().length > 0);
