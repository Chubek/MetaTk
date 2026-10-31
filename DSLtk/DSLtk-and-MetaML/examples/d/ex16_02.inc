auto s = new MetaML.Session();
auto r = s.eval("1 + true");
assert(r.is_err());
assert(r.unwrap_err().length > 0);
