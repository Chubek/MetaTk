use round, testing;
equal(round_div(7, 2), 4); equal(round_div(-7, 2), -4); equal(round_div(5, 3), 2);
equal(round_half_even(5, 2), 2); equal(round_half_even(7, 2), 4); equal(round_half_even(-5, 2), -2);
equal(floor_to(17, 5), 15); equal(ceil_to(17, 5), 20); equal(round_to(1234, 100), 1200);
equal(trunc_to(-17, 5), -15); equal(floor_to(-17, 5), -20);
equal(percent(1, 3), 33); equal(percent(2, 3), 67); equal(permille(1, 8), 125);
equal(scale(10, 2, 3), 7); equal(trunc_div(7, 2), 3);
print 1;
