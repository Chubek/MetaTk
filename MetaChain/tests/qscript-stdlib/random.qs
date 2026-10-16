use random, testing;
equal(seed(42), -4767286540954276203);
equal(xorshift(1), 1082269761); equal(lcg(1), 7806831264735756412);
let s = seed(7);
let i = 0;
while (i < 200) {
    s = step(s);
    between(range(s, 1, 6), 1, 6);
    between(below(s, 10), 0, 9);
    between(coin(s), 0, 1);
    between(fraction(s), 0, 65535);
    i++;
}
not_equal(value(step(s)), value(s));
print 1;
