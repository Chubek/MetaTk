auto x = Maybe!int(4);
auto y = x.map((int n) => n * 3);
assert(y.or_else(0) == 12);
assert(Maybe!int.init.or_else(9) == 9);
