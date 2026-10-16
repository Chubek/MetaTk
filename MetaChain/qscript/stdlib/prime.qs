// prime: primality and factorization by trial division.
//
//     use prime;
//     print is_prime(97), next_prime(100), nth_prime(10), totient(36);
//
// Trial division is exact for every int64 but slows down for large
// primes; ntheory::is_probable_prime is the fast deterministic test.
fn is_prime(n) {
    if (n < 2) return 0;
    if (n < 4) return 1;
    if (n % 2 == 0 || n % 3 == 0) return 0;
    for (let i = 5; i <= n / i; i += 6) {
        if (n % i == 0 || n % (i + 2) == 0) return 0;
    }
    return 1;
}
fn next_prime(n) {
    if (n < 2) return 2;
    let c = n + 1;
    while (!is_prime(c)) c++;
    return c;
}
fn prev_prime(n) {
    assert n > 2;
    let c = n - 1;
    while (!is_prime(c)) c--;
    return c;
}
// The k-th prime, 1-based: nth_prime(1) == 2.
fn nth_prime(k) {
    assert k >= 1;
    let p = 2;
    for (let i = 1; i < k; i++) p = next_prime(p);
    return p;
}
// Number of primes <= n.
fn prime_count(n) {
    let c = 0;
    for (let i = 2; i <= n; i++) if (is_prime(i)) c++;
    return c;
}
fn smallest_factor(n) {
    assert n >= 2;
    if (n % 2 == 0) return 2;
    for (let i = 3; i <= n / i; i += 2) if (n % i == 0) return i;
    return n;
}
fn largest_factor(n) {
    assert n >= 2;
    let f = 1;
    while (n > 1) {
        f = smallest_factor(n);
        while (n % f == 0) n = n / f;
    }
    return f;
}
// Number of distinct prime factors, and with multiplicity.
fn omega(n) {
    let c = 0;
    while (n > 1) {
        let p = smallest_factor(n);
        while (n % p == 0) n = n / p;
        c++;
    }
    return c;
}
fn bigomega(n) {
    let c = 0;
    while (n > 1) { n = n / smallest_factor(n); c++; }
    return c;
}
// Euler's phi.
fn totient(n) {
    let r = n;
    let m = n;
    for (let p = 2; p <= m / p; p++) {
        if (m % p == 0) {
            while (m % p == 0) m = m / p;
            r = r - r / p;
        }
    }
    if (m > 1) r = r - r / m;
    return r;
}
