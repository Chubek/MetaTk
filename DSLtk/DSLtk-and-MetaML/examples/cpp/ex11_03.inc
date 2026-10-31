dsl::Span a{0, 2, 1, 1, "sample.dsl"};
dsl::Span b{3, 5, 1, 4, "sample.dsl"};
auto all = dsl::merge_spans(a, b);
assert(all.begin == 0 && all.end == 5);
