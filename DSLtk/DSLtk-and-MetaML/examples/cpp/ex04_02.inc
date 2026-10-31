using R = dsl::Result<int, std::string>;
auto good = R::from_ok(42);
auto bad = R::from_err("invalid number");
assert(good.unwrap() == 42);
assert(bad.unwrap_err() == "invalid number");
