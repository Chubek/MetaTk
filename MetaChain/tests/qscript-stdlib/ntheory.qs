use ntheory, testing;
equal(mod(-7, 3), 2);
equal(mod_add(9223372036854775806, 9223372036854775806, 9223372036854775807), 9223372036854775805);
equal(mod_mul(9223372036854775806, 2, 9223372036854775807), 9223372036854775805);
equal(mod_pow(2, 100, 1000000007), 976371285);
equal(mod_inv(3, 11), 4); equal(mod_inv(2, 4), 0);
equal(is_coprime(8, 15), 1);
equal(crt(2, 3, 3, 5), 8);
equal(jacobi(2, 7), 1); equal(jacobi(3, 7), -1); equal(jacobi(7, 21), 0);
equal(is_probable_prime(2305843009213693951), 1);     // 2**61 - 1
equal(is_probable_prime(3215031751), 0);              // strong pseudoprime to 2, 3, 5, 7
equal(is_probable_prime(9223372036854775783), 1);     // largest int64 prime
equal(is_probable_prime(1), 0); equal(is_probable_prime(2), 1);
print 1;
