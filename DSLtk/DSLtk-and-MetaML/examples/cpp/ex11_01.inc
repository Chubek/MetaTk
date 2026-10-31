dsl::Preprocessor p;
p.define("LIMIT", "42");
assert(p.expand("LIMIT + 1") == "42 + 1");
