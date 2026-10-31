auto n = node!"sub"(
  leaf!"integer"(9), leaf!"integer"(4));
assert(!n.is_leaf());
assert(n.size() == 2);
assert(n.child(0).value() == "9");
