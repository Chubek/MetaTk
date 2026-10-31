auto r = MetaML.parse_expression("(1 + 2", "bad.ml");
assert(r.is_err());
assert(r.unwrap_err().length > 0);
