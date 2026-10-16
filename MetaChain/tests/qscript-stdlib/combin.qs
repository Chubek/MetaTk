use combin, testing;
equal(factorial(0), 1); equal(factorial(20), 2432902008176640000);
equal(double_factorial(7), 105);
equal(binomial(52, 5), 2598960); equal(binomial(10, 11), 0); equal(binomial(60, 30), 118264581564861424);
equal(multichoose(3, 2), 6); equal(permutations(5, 2), 20); equal(rising(3, 3), 60);
equal(catalan(10), 16796); equal(fibonacci(90), 2880067194370816120);
equal(lucas(10), 123); equal(derangements(4), 9); equal(derangements(1), 0);
equal(stirling2(5, 2), 15);
print 1;
