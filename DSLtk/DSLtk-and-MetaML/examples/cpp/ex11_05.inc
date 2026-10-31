dsl::Diagnostic d;
d.code = "APP001";
d.message = "expected a positive size";
assert(d.headline() == "error[APP001]: expected a positive size");
