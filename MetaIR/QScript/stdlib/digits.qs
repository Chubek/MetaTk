// digits: decimal digits of integers.
//
//     use digits;
//     print count(12345), sum(999), reverse(1230), is_palindrome(12321), digit(9876, 2);
//
// Functions work on the absolute value unless noted; reverse keeps the sign.
fn count(n) {
    if (n == 0) return 1;
    let c = 0;
    while (n != 0) { n = n / 10; c++; }
    return c;
}
fn count_base(n, base) {
    assert base >= 2;
    if (n == 0) return 1;
    let c = 0;
    while (n != 0) { n = n / base; c++; }
    return c;
}
fn sum(n) {
    let s = 0;
    while (n != 0) { s += n % 10; n = n / 10; }
    return s < 0 ? -s : s;
}
fn product(n) {
    let p = 1;
    if (n == 0) return 0;
    while (n != 0) { let d = n % 10; p *= d < 0 ? -d : d; n = n / 10; }
    return p;
}
fn reverse(n) {
    let r = 0;
    while (n != 0) { r = r * 10 + n % 10; n = n / 10; }
    return r;
}
fn is_palindrome(n) { return n >= 0 && reverse(n) == n; }
// The i-th decimal digit counting from the right, starting at 0.
fn digit(n, i) {
    while (i > 0) { n = n / 10; i--; }
    let d = n % 10;
    return d < 0 ? -d : d;
}
fn digital_root(n) {
    assert n >= 0;
    return n == 0 ? 0 : 1 + (n - 1) % 9;
}
// Digits of b appended to a (both non-negative).
fn concat(a, b) {
    let shift = 1;
    let t = b;
    while (t >= 10) { t = t / 10; shift *= 10; }
    return a * shift * 10 + b;
}
fn is_armstrong(n) {
    assert n >= 0;
    let k = count(n);
    let s = 0;
    let t = n;
    while (t > 0) { s += (t % 10) ** k; t = t / 10; }
    return s == n;
}
fn is_harshad(n) { return n > 0 && n % sum(n) == 0; }
