// fixed: Q16.16 fixed-point arithmetic in a 64-bit integer.
//
//     use fixed;
//     let r = from_ratio(3, 2);                  // 1.5
//     print to_milli(mul(r, r)), to_milli(sqrt(from_int(2)));   // 2250 1414
//
// A value v represents v / 65536. Products are exact up to about 2**47 in
// magnitude; mul/div truncate toward zero; floor/ceil/round return fixed
// values.
use math;

const SHIFT = 16;
const ONE = 65536;
const HALF = 32768;

fn from_int(n) { return n * ONE; }
fn to_int(x) { return x / ONE; }
fn from_ratio(n, d) { return n * ONE / d; }
fn from_milli(m) { return m * ONE / 1000; }
fn to_milli(x) { return x * 1000 / ONE; }
fn mul(a, b) { return a * b / ONE; }
fn div(a, b) { return a * ONE / b; }
fn recip(x) { return ONE * ONE / x; }
fn floor(x) { return x & -65536; }
fn ceil(x) { return -floor(-x); }
fn round(x) { return floor(x + HALF); }
fn frac(x) { return x - floor(x); }
fn sqrt(x) { assert x >= 0; return isqrt(x * ONE); }
fn lerp(a, b, t) { return a + mul(b - a, t); }
