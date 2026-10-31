auto p = *dsl::ch('x');
auto r = dsl::run_parser(p, "xxx");
assert(r.value.has_value());
assert(r.value->size() == 3);
