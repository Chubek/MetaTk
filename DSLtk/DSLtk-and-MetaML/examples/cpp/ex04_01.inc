dsl::Maybe<int> x(4);
auto y = x.map([](int n) { return n * 3; });
assert(y.or_else(0) == 12);
assert(dsl::Maybe<int>{}.or_else(9) == 9);
