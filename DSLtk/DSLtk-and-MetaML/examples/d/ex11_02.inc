auto p = new Preprocessor();
p.define_function("TWICE", ["x"], "(x + x)");
assert(p.expand("TWICE(3)") == "(3 + 3)");
