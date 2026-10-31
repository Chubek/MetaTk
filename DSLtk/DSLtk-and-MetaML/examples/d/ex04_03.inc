alias R = Result!(int, string);
auto r = R.from_ok(6).map((int n) => n * 7);
assert(r.is_ok());
assert(r.unwrap() == 42);
