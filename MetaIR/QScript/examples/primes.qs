// The first ten primes, a twin-prime count, and a Mersenne prime check.
use prime, ntheory;

let p = 2;
for (let i = 0; i < 10; i++) { print p; p = next_prime(p); }

fn twin_primes_below(n) {
    let count = 0;
    for (let k = 3; k + 2 < n; k += 2) count++ if is_prime(k) && is_prime(k + 2);
    return count;
}
print twin_primes_below(1000);                 // 35
return is_probable_prime(2 ** 61 - 1);         // 1
