// ntheory: modular arithmetic and number theory, overflow-safe.
//
//     use ntheory;
//     print mod_pow(2, 100, 1000000007), mod_inv(3, 11), is_probable_prime(2305843009213693951);
//
// Moduli are positive. mod_add/mod_mul never overflow, so any modulus up to
// INT_MAX works. is_probable_prime is a deterministic Miller-Rabin test,
// exact for every int64.
use math;

fn mod(a, m) { let r = a % m; return r < 0 ? r + m : r; }
fn mod_add(a, b, m) {
    a = mod(a, m); b = mod(b, m);
    return a >= m - b ? a - (m - b) : a + b;
}
fn mod_sub(a, b, m) { return mod_add(a, m - mod(b, m), m); }
fn mod_mul(a, b, m) {
    a = mod(a, m); b = mod(b, m);
    if (m <= 3037000499) return a * b % m;      // the product fits in int64
    // Double-and-add with the overflow-safe addition inlined.
    let r = 0;
    while (b > 0) {
        if (b & 1) r = r >= m - a ? r - (m - a) : r + a;
        a = a >= m - a ? a - (m - a) : a + a;
        b = b >> 1;
    }
    return r;
}
fn mod_pow(base, exp, m) {
    assert exp >= 0;
    if (m == 1) return 0;
    let r = 1;
    base = mod(base, m);
    while (exp > 0) {
        if (exp & 1) r = mod_mul(r, base, m);
        base = mod_mul(base, base, m);
        exp = exp >> 1;
    }
    return r;
}
// Inverse of a modulo m, or 0 when gcd(a, m) != 1.
fn mod_inv(a, m) {
    let t = 0; let nt = 1;
    let r = m; let nr = mod(a, m);
    while (nr != 0) {
        let q = r / nr;
        let tmp = t - q * nt; t = nt; nt = tmp;
        tmp = r - q * nr; r = nr; nr = tmp;
    }
    if (r > 1) return 0;
    return mod(t, m);
}
fn is_coprime(a, b) { return gcd(a, b) == 1; }
// x with x = a1 (mod m1) and x = a2 (mod m2) for coprime m1, m2, in
// [0, m1*m2); -1 if the moduli are not coprime.
fn crt(a1, m1, a2, m2) {
    if (gcd(m1, m2) != 1) return -1;
    let k = mod_mul(mod_sub(a2, a1, m2), mod_inv(m1, m2), m2);
    return mod(a1, m1) + m1 * k;
}
// Jacobi symbol (a/n) for odd n > 0: -1, 0 or 1.
fn jacobi(a, n) {
    assert n > 0 && (n & 1) == 1;
    a = mod(a, n);
    let result = 1;
    while (a != 0) {
        while ((a & 1) == 0) {
            a = a >> 1;
            let r = n & 7;
            if (r == 3 || r == 5) result = -result;
        }
        let t = a; a = n; n = t;
        if ((a & 3) == 3 && (n & 3) == 3) result = -result;
        a = a % n;
    }
    return n == 1 ? result : 0;
}
fn _witness(n, d, s, a) {
    let x = mod_pow(a, d, n);
    if (x == 1 || x == n - 1) return 0;
    for (let i = 1; i < s; i++) {
        x = mod_mul(x, x, n);
        if (x == n - 1) return 0;
    }
    return 1;
}
fn is_probable_prime(n) {
    if (n < 2) return 0;
    if (n % 2 == 0) return n == 2;
    if (n % 3 == 0) return n == 3;
    let d = n - 1;
    let s = 0;
    while ((d & 1) == 0) { d = d >> 1; s++; }
    // Bases 2..37 decide every n < 3.3e24, so every int64.
    let a = 2;
    while (a <= 37) {
        if (a % n != 0 && _witness(n, d, s, a)) return 0;
        a = a == 2 ? 3 : a == 3 ? 5 : a == 5 ? 7 : a == 7 ? 11 : a == 11 ? 13 : a == 13 ? 17 : a == 17 ? 19 : a == 19 ? 23 : a == 23 ? 29 : a == 29 ? 31 : a == 31 ? 37 : 41;
    }
    return 1;
}
