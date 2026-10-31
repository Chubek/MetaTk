int calls = 0;
dsl::Lazy<int> x([&] { ++calls; return 42; });
assert(!x.is_computed());
assert(x.get() == 42);
assert(x.force() == 42 && calls == 1);
