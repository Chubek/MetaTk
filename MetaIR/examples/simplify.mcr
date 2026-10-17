; Identities for addition and multiplication by the integers 0 and 1.
add_zero: add($x, 0) => $x
mul_one: mul($x, 1) => $x
%%
add(mul(42, 1), 0)
