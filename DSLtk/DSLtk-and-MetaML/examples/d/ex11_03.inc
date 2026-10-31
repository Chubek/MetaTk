auto a = Span(0, 2, 1, 1, "sample.dsl");
auto b = Span(3, 5, 1, 4, "sample.dsl");
auto all = merge_spans(a, b);
assert(all.begin == 0 && all.end == 5);
