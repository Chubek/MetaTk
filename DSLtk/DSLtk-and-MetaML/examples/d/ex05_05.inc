auto p = optional(ch('x'));
auto r = run_parser(p, "");
assert(!r.value.isNull);
assert(r.value.get.isNull);
