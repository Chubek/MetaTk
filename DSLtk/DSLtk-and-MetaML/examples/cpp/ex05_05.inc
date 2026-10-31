auto p = dsl::optional(dsl::ch('x'));
auto r = dsl::run_parser(p, "");
assert(r.value.has_value());
assert(!r.value->has_value());
