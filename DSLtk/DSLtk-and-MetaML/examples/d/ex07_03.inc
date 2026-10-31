auto xs = parse_sexpressions("(a 1) (b 2)");
assert(xs.length == 2);
assert(xs[1].items[0].text == "b");
