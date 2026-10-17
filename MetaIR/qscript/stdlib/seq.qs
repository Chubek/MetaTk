// seq: figurate numbers and integer sequences.
//
//     use seq;
//     print triangular(10), is_square(144), collatz_steps(27), sigma(28), is_perfect(28);
use math;

fn triangular(n) { return n * (n + 1) / 2; }
fn pentagonal(n) { return n * (3 * n - 1) / 2; }
fn hexagonal(n) { return n * (2 * n - 1); }
fn is_square(x) { if (x < 0) return 0; let r = isqrt(x); return r * r == x; }
fn is_triangular(x) { return x >= 0 && is_square(8 * x + 1); }
fn is_fibonacci(x) {
    if (x < 0) return 0;
    let a = 0; let b = 1;
    while (a < x) { let t = a + b; a = b; b = t; }
    return a == x;
}
fn tribonacci(n) {
    let a = 0; let b = 0; let c = 1;
    if (n < 2) return 0;
    for (let i = 2; i < n; i++) { let t = a + b + c; a = b; b = c; c = t; }
    return c;
}
fn pell(n) {
    let a = 0; let b = 1;
    while (n > 0) { let t = 2 * b + a; a = b; b = t; n--; }
    return a;
}
// Steps for n to reach 1 under the Collatz map.
fn collatz_steps(n) {
    assert n >= 1;
    let s = 0;
    while (n != 1) { n = (n & 1) ? 3 * n + 1 : n / 2; s++; }
    return s;
}
// Sum of all positive divisors.
fn sigma(n) {
    assert n >= 1;
    let s = 0;
    for (let d = 1; d <= n / d; d++) {
        if (n % d == 0) { s += d; if (d != n / d) s += n / d; }
    }
    return s;
}
fn aliquot(n) { return sigma(n) - n; }
fn is_perfect(n) { return n > 1 && aliquot(n) == n; }
fn is_abundant(n) { return aliquot(n) > n; }
fn arithmetic(a, d, n) { return a + n * d; }
fn arithmetic_sum(a, d, n) { return n * (2 * a + (n - 1) * d) / 2; }
fn geometric(a, r, n) { return a * r ** n; }
fn geometric_sum(a, r, n) {
    let s = 0; let t = a;
    for (let i = 0; i < n; i++) { s += t; t *= r; }
    return s;
}
fn is_happy(n) {
    assert n >= 1;
    while (n != 1 && n != 4) {
        let s = 0;
        while (n > 0) { let d = n % 10; s += d * d; n = n / 10; }
        n = s;
    }
    return n == 1;
}
