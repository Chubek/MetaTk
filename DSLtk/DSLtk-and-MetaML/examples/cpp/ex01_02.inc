auto twice = dsl::pipe([](int x) { return x * 2; });
auto answer = 21 | twice;
assert(answer == 42);
