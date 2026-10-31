auto g = create_peg_definition();
g.add_rule!"[0-9]+"();
auto r = g.parse("12?");
assert(r.failed());
assert(r.error_offset() == 2);
