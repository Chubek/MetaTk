auto s = new MetaML.Session();
auto r = s.eval("missing", "policy.ml");
assert(r.is_err());
auto text = s.render(r.unwrap_err()[0]);
assert(text.length > 0);
