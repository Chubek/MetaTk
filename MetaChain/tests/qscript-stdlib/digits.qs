use digits, testing;
equal(count(0), 1); equal(count(-12345), 5); equal(count_base(255, 16), 2);
equal(sum(999), 27); equal(sum(-123), 6); equal(product(234), 24);
equal(reverse(1230), 321); equal(reverse(-12), -21);
equal(is_palindrome(12321), 1); equal(is_palindrome(-121), 0);
equal(digit(9876, 2), 8); equal(digital_root(9875), 2);
equal(concat(12, 345), 12345); equal(concat(7, 0), 70);
equal(is_armstrong(153), 1); equal(is_harshad(18), 1);
print 1;
