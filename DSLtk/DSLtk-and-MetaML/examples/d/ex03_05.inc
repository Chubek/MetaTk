auto n = node!"add"(leaf!"n"(1), leaf!"n"(2));
assert(n.dump() == "(add (n 1) (n 2))");
