// combin: combinatorics and classic integer sequences.
//
//     use combin;
//     print factorial(10), binomial(52, 5), catalan(10), fibonacci(90);
//
// Results wrap past int64: factorial is exact up to 20, fibonacci up to 92,
// binomial while every intermediate C(n-k+i, i) * (n-k+i+1) fits.
fn factorial(n) {
    assert n >= 0;
    let r = 1;
    for (let i = 2; i <= n; i++) r *= i;
    return r;
}
fn double_factorial(n) {
    assert n >= -1;
    let r = 1;
    while (n > 1) { r *= n; n -= 2; }
    return r;
}
fn binomial(n, k) {
    if (k < 0 || k > n) return 0;
    if (k > n - k) k = n - k;
    let r = 1;
    for (let i = 1; i <= k; i++) r = r * (n - k + i) / i;
    return r;
}
fn multichoose(n, k) { return binomial(n + k - 1, k); }
fn permutations(n, k) {
    if (k < 0 || k > n) return 0;
    let r = 1;
    for (let i = 0; i < k; i++) r *= n - i;
    return r;
}
fn rising(x, k) {
    let r = 1;
    for (let i = 0; i < k; i++) r *= x + i;
    return r;
}
fn catalan(n) { return binomial(2 * n, n) / (n + 1); }
fn fibonacci(n) {
    assert n >= 0;
    let a = 0; let b = 1;
    while (n > 0) { let t = a + b; a = b; b = t; n--; }
    return a;
}
fn lucas(n) {
    assert n >= 0;
    let a = 2; let b = 1;
    while (n > 0) { let t = a + b; a = b; b = t; n--; }
    return a;
}
fn derangements(n) {
    if (n == 0) return 1;
    let a = 1; let b = 0;
    for (let i = 2; i <= n; i++) { let c = (i - 1) * (a + b); a = b; b = c; }
    return b;
}
// Stirling numbers of the second kind via the row recurrence, k <= 16.
fn stirling2(n, k) {
    if (n == k) return 1;
    if (k == 0 || k > n) return 0;
    return k * stirling2(n - 1, k) + stirling2(n - 1, k - 1);
}
