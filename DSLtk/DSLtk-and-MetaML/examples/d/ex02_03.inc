auto zero = predicate((int x) => x == 0);
auto nonzero = ~zero;
assert(nonzero(7));
assert(!nonzero(0));
