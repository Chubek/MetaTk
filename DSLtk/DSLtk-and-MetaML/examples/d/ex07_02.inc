auto s = SExpr.atom("two words");
auto text = s.dump();
auto back = parse_sexpr(text);
assert(back.text == "two words");
assert(back.quoted);
