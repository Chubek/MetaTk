auto r = metaml::parse_expression("(1 + 2", "bad.mml");
assert(r.is_err());
assert(!r.unwrap_err().empty());
