int calls;
auto square = memoize!(int, int)((int n) {
  ++calls; return n * n;
});
assert(square(6) == 36 && square(6) == 36);
assert(calls == 1);
square.clear_cache();
