auto ab = ch('a') & ch('b');
auto ac = ch('a') & ch('c');
auto p = try_parse(ab) | ac;
assert(!run_parser(p, "ac").value.isNull);
