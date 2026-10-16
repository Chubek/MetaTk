// checked: overflow detection, saturating and trapping arithmetic.
//
//     use checked;
//     let a = 3037000500;
//     print mul_ok(a, a);                // 0: a * a overflows
//     print add_sat(INT_MAX, 1);         // INT_MAX
//     print add_checked(a, 1);           // traps if the sum overflows
//
// *_ok return 1 when the operation is exact, *_sat clamp to the int64
// range, *_checked assert exactness.
const INT_MAX = 9223372036854775807;
const INT_MIN = -9223372036854775808;

fn add_ok(a, b) { let s = a + b; return ((a ^ s) & (b ^ s)) >= 0; }
fn sub_ok(a, b) { let d = a - b; return ((a ^ b) & (a ^ d)) >= 0; }
fn mul_ok(a, b) {
    if (a == 0 || b == 0) return 1;
    if (a == -1) return b != INT_MIN;
    if (b == -1) return a != INT_MIN;
    return (a * b) / b == a;
}
fn neg_ok(a) { return a != INT_MIN; }
fn div_ok(a, b) { return b != 0 && !(a == INT_MIN && b == -1); }

fn add_sat(a, b) {
    if (add_ok(a, b)) return a + b;
    return a > 0 ? INT_MAX : INT_MIN;
}
fn sub_sat(a, b) {
    if (sub_ok(a, b)) return a - b;
    return a >= 0 ? INT_MAX : INT_MIN;
}
fn mul_sat(a, b) {
    if (mul_ok(a, b)) return a * b;
    return (a < 0) == (b < 0) ? INT_MAX : INT_MIN;
}
fn neg_sat(a) { return a == INT_MIN ? INT_MAX : -a; }
fn abs_sat(a) { return a < 0 ? neg_sat(a) : a; }

fn add_checked(a, b) { assert add_ok(a, b); return a + b; }
fn sub_checked(a, b) { assert sub_ok(a, b); return a - b; }
fn mul_checked(a, b) { assert mul_ok(a, b); return a * b; }
fn div_checked(a, b) { assert div_ok(a, b); return a / b; }
