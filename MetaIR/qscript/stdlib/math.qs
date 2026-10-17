// math: integer arithmetic.
//
//     use math;
//     print gcd(84, 36), isqrt(1000), clamp(42, 0, 10);   // 12 31 10
//
// All arithmetic is signed 64-bit and wraps on overflow, so abs(INT_MIN)
// is INT_MIN. Functions that need a precondition (isqrt of a negative
// number, ilog2 of zero) assert it.
const INT_MAX = 9223372036854775807;
const INT_MIN = -9223372036854775808;

fn abs(x) { return x < 0 ? -x : x; }
fn sign(x) { return (x > 0) - (x < 0); }
fn min(a, b) { return a < b ? a : b; }
fn max(a, b) { return a > b ? a : b; }
fn min3(a, b, c) { return min(min(a, b), c); }
fn max3(a, b, c) { return max(max(a, b), c); }
fn clamp(x, lo, hi) { return x < lo ? lo : x > hi ? hi : x; }
fn square(x) { return x * x; }
fn cube(x) { return x * x * x; }
fn is_even(x) { return (x & 1) == 0; }
fn is_odd(x) { return (x & 1) == 1; }
// |a - b| without overflow for operands of the same sign.
fn dist(a, b) { return a > b ? a - b : b - a; }

fn gcd(a, b) {
    a = abs(a); b = abs(b);
    while (b != 0) { let t = a % b; a = b; b = t; }
    return a;
}
fn lcm(a, b) {
    if (a == 0 || b == 0) return 0;
    return abs(a / gcd(a, b) * b);
}
fn pow(base, exp) { return base ** exp; }

// Floor of the square root (Newton's method, no overflow).
fn isqrt(n) {
    assert n >= 0;
    if (n < 4) return n == 0 ? 0 : 1;
    let x = n;
    let y = n / 2 + 1;
    while (y < x) { x = y; y = (x + n / x) / 2; }
    return x;
}
// Integer cube root, rounded toward zero (binary search).
fn icbrt(n) {
    let negative = n < 0;
    let m = negative ? -n : n;
    let lo = 0;
    let hi = 2097151;          // 2**21 - 1, the largest cube root of an int64
    while (lo < hi) {
        let mid = (lo + hi + 1) / 2;
        if (mid * mid * mid <= m) lo = mid; else hi = mid - 1;
    }
    return negative ? -lo : lo;
}
fn ilog2(n) {
    assert n > 0;
    let r = 0;
    while (n > 1) { n = n >> 1; r++; }
    return r;
}
fn ilog10(n) {
    assert n > 0;
    let r = 0;
    while (n >= 10) { n = n / 10; r++; }
    return r;
}
// Division rounding toward negative infinity / positive infinity.
fn div_floor(a, b) {
    let q = a / b;
    if (a % b != 0 && (a < 0) != (b < 0)) q--;
    return q;
}
fn div_ceil(a, b) {
    let q = a / b;
    if (a % b != 0 && (a < 0) == (b < 0)) q++;
    return q;
}
// Remainder with the sign of the divisor (Python's %).
fn mod_floor(a, b) {
    let r = a % b;
    if (r != 0 && (r < 0) != (b < 0)) r += b;
    return r;
}
// floor((a + b) / 2) without overflow.
fn average(a, b) {
    let d = a ^ b;
    return (a & b) + (d >= 0 ? d >> 1 : ~(~d >> 1));
}
