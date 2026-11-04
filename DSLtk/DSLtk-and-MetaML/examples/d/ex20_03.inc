auto r = MetaML.parse("val x := 42", "unit.mml");
assert(r.ok());
assert(r.program.items.length == 1);
