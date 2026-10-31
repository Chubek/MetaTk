auto p = ch('x').many();
auto r = run_parser(p, "xxx");
assert(!r.value.isNull);
assert(r.value.get.length == 3);
