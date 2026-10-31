int calls;
auto x = Lazy!int(() { ++calls; return 42; });
assert(!x.is_computed());
assert(x.get() == 42);
assert(x.force() == 42 && calls == 1);
