auto before = dsl::node<"box">(dsl::leaf<"n">(1));
auto after = before;
after.child(0) = dsl::leaf<"n">(2);
assert(before.child(0).value() == "1");
assert(after.child(0).value() == "2");
