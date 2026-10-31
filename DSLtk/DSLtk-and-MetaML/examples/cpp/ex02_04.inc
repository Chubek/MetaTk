auto table = dsl::match(
  dsl::when<1>([](int) { return 10; }),
  dsl::otherwise([](int) { return -1; }));
assert(table(1) == 10);
assert(table(2) == -1);
