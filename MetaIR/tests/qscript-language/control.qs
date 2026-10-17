// Control flow, functions, constants and operators.
const LIMIT = 4;
fn fact(n: int): int { if (n <= 1) { return 1; } return n * fact(n - 1); }
fn describe(x: int): str { return x > 0 ? "positive" : x < 0 ? "negative" : "zero"; }
fn join_all(parts: list<str>, sep: str): str {
    let out = "";
    let first = true;
    for (my p in parts) {
        if (!first) { out = out ~ sep; }
        out = out ~ p;
        first = false;
    }
    return out;
}
print fact(5);
print describe(-3) ~ " " ~ describe(0) ~ " " ~ describe(8);
print join_all(["a", "b", "c"], "/");
print 2 ** 10;
print 7 / 2;
print -7 % 3;
print 1 << 4 | 1;
print !0 + !5;
print ~0;
print (3 > 2) && (2 > 1);
print 0 || 9;
let i = 0;
while (i < LIMIT) { i++; }
do { i--; } while (i > 2);
print i;
let steps = 0;
for (my k = 0; k < 10; k++) {
    if (k == 6) { break; }
    if (k % 2 == 1) { continue; }
    steps++;
}
print steps;
let flag = 0;
if (steps > 5 && steps < 10) { flag = 1; } else { flag = 2; }
print flag;
assert fact(3) == 6;
return fact(LIMIT);
