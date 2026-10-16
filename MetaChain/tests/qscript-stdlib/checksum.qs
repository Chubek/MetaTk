use checksum, testing;
let c = CRC32_INIT;
let c8 = 0;
let a = ADLER32_INIT;
let ch = '1';
while (ch <= '9') { c = crc32_byte(c, ch); c8 = crc8_byte(c8, ch); ch++; }
equal(crc32_final(c), 3421780262);
equal(c8, 244);
// "Wikipedia"
a = adler32_byte(a, 'W'); a = adler32_byte(a, 'i'); a = adler32_byte(a, 'k');
a = adler32_byte(a, 'i'); a = adler32_byte(a, 'p'); a = adler32_byte(a, 'e');
a = adler32_byte(a, 'd'); a = adler32_byte(a, 'i'); a = adler32_byte(a, 'a');
equal(a, 300286872);
let f = 0;
ch = 'a';
while (ch <= 'e') { f = fletcher16_byte(f, ch); ch++; }
equal(f, 51440);
equal(luhn_valid(79927398713), 1); equal(luhn_valid(79927398710), 0);
equal(luhn_digit(7992739871), 3);
equal(mod97_valid(98), 1); equal(mod97_valid(97), 0);
print 1;
