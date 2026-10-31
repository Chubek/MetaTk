auto n = leaf!"name"("two words");
auto sexpr = ast_to_sexpr(n);
auto back = sexpr_to_ast(sexpr);
assert(back == n);
