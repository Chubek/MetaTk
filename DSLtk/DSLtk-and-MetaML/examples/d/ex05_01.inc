auto p = ch('a');
assert(!run_parser(p, "a").value.isNull);
assert(run_parser(p, "ab").value.isNull);
