use testing;
equal(1, 1); not_equal(1, 2); less(1, 2); between(5, 1, 9); is_true(3); is_false(0);
let passed = 0;
passed = check(passed, 1, 101);
passed = check(passed, 2 + 2 == 4, 102);
equal(passed, 2);
print 1;
