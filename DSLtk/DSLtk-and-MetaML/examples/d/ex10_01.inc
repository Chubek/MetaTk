alias Number = StaticRegex!"[0-9]+";
assert(Number.matches("123"));
assert(!Number.matches("123x"));
