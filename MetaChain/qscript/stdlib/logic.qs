// logic: boolean helpers. Any non-zero integer is true; results are 0 or 1.
//
//     use logic;
//     print xor(2, 0), implies(0, 5), majority(1, 0, 1), cmp(3, 7), select(1, 10, 20);
//
// (`and`, `or` and `not` are operators, so they are not functions here.)
fn truthy(x) { return x != 0; }
fn xor(a, b) { return (a != 0) != (b != 0); }
fn nand(a, b) { return !(a && b); }
fn nor(a, b) { return !(a || b); }
fn xnor(a, b) { return (a != 0) == (b != 0); }
fn iff(a, b) { return xnor(a, b); }
fn implies(a, b) { return !a || b; }
fn select(c, a, b) { return c ? a : b; }
fn majority(a, b, c) { return (a != 0) + (b != 0) + (c != 0) >= 2; }
fn count3(a, b, c) { return (a != 0) + (b != 0) + (c != 0); }
fn all3(a, b, c) { return a && b && c; }
fn any3(a, b, c) { return a || b || c; }
fn none3(a, b, c) { return !(a || b || c); }
// Three-way comparison: -1, 0 or 1.
fn cmp(a, b) { return (a > b) - (a < b); }
