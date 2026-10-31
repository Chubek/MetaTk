auto a = dsl::leaf<"n">(1);
auto b = dsl::leaf<"n">(1);
auto c = dsl::leaf<"other">(1);
assert(a == b);
assert(!(a == c));
