using R = dsl::Result<int, std::string>;
auto r = R::from_ok(6).map([](int n) { return n * 7; });
assert(r.is_ok());
assert(r.unwrap() == 42);
