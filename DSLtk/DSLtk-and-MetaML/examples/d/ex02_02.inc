auto positive = predicate((int x) => x > 0);
auto even = predicate((int x) => x % 2 == 0);
auto valid = positive & even;
assert(valid(8));
assert(!valid(-2));
