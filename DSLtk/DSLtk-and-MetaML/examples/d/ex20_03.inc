auto r = MetaML.parse("val x := 42", "unit.ml");
assert(r.ok());
assert(r.program.items.length == 1);
