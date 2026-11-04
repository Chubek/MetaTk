auto r = metaml::parse("val x := 42", "unit.mml");
assert(r.ok());
assert(r.program.items.size() == 1);
