use logic, testing;
equal(truthy(-3), 1); equal(xor(2, 0), 1); equal(xor(2, 3), 0);
equal(nand(1, 1), 0); equal(nor(0, 0), 1); equal(xnor(5, 6), 1); equal(iff(0, 1), 0);
equal(implies(0, 0), 1); equal(implies(1, 0), 0);
equal(select(1, 10, 20), 10); equal(select(0, 10, 20), 20);
equal(majority(1, 0, 1), 1); equal(count3(1, 0, 7), 2);
equal(all3(1, 2, 3), 1); equal(any3(0, 0, 0), 0); equal(none3(0, 0, 0), 1);
equal(cmp(3, 7), -1); equal(cmp(7, 7), 0); equal(cmp(9, 7), 1);
print 1;
