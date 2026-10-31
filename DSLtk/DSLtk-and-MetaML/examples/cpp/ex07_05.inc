auto r = dsl::sexpr_rule(
  "(rewrite add-zero (add ?x 0) ?x)");
assert(r.name == "add-zero");
assert(r.rhs.text == "?x");
