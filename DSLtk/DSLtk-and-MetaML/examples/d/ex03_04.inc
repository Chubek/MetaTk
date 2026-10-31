auto a = leaf!"n"(1);
auto b = leaf!"n"(1);
auto c = leaf!"other"(1);
assert(a == b);
assert(!(a == c));
