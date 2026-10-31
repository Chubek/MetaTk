int calls = 0;
auto square = dsl::memoize<int, int>([&](int n) {
  ++calls; return n * n;
});
assert(square(6) == 36 && square(6) == 36);
assert(calls == 1);
square.clear_cache();
