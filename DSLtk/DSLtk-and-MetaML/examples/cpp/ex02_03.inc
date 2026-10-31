auto zero = dsl::predicate([](int x) { return x == 0; });
auto nonzero = !zero;
assert(nonzero(7));
assert(!nonzero(0));
