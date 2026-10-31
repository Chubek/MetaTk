auto p = dsl::ch('a');
assert(dsl::run_parser(p, "a").value.has_value());
assert(!dsl::run_parser(p, "ab").value.has_value());
