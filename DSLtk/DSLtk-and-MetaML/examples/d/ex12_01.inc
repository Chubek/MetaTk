auto n = leaf!"n"(42);
int seen;
auto v = derive_visitor(n,
  on!"n"((ref ASTNode x) { ++seen; }));
v.walk(n);
assert(seen == 1);
