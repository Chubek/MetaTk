auto p = dsl::ch('a') & dsl::ch('b');
auto r = dsl::run_parser(p, "ab");
assert(r.value.has_value());
assert(r.value->first == 'a');
