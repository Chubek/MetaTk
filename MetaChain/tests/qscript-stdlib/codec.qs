use codec, testing;
equal(zigzag(-3), 5); equal(zigzag(3), 6); equal(unzigzag(5), -3); equal(unzigzag(zigzag(-9223372036854775808)), -9223372036854775808);
equal(gray(6), 5); equal(ungray(5), 6); equal(ungray(gray(123456789)), 123456789);
equal(bcd(1234), 0x1234); equal(unbcd(0x9876), 9876);
equal(varint_size(0), 1); equal(varint_size(127), 1); equal(varint_size(300), 2); equal(varint_size(-1), 10);
let m = morton(5, 9);
equal(morton_x(m), 5); equal(morton_y(m), 9); equal(morton(1, 0), 1); equal(morton(0, 1), 2);
print 1;
