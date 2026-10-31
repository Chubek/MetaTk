int count;
auto g = create_peg_definition();
g.add_rule!"[0-9]+"((PEGMatch m) {
  assert(m.value == "42"); ++count;
});
assert(g.parse("42").ok() && count == 1);
