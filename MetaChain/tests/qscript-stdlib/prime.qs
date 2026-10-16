use prime, testing;
equal(is_prime(1), 0); equal(is_prime(2), 1); equal(is_prime(97), 1); equal(is_prime(91), 0);
equal(is_prime(2147483647), 1);
equal(next_prime(100), 101); equal(prev_prime(100), 97); equal(nth_prime(10), 29);
equal(prime_count(100), 25);
equal(smallest_factor(91), 7); equal(largest_factor(600851475143), 6857);
equal(omega(360), 3); equal(bigomega(360), 6);
equal(totient(36), 12); equal(totient(97), 96);
print 1;
