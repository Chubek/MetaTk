auto p = ch('a') & ch('b');
auto r = run_parser(p, "ab");
assert(!r.value.isNull);
