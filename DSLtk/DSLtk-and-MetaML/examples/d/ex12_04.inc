auto schema = leaf!"n"(0);
auto v = derive_visitor(schema,
  on!"n"((ref ASTNode x) {}));
auto n = leaf!"n"(8);
v.visit(n);
assert(n.value() == "8");
