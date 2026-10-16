use endian;
assert(swap16(0x1234)==0x3412);
assert(swap32(0x12345678)==0x78563412);
assert(swap64(0x0123456789abcdef)==(0x6fcdab8967452301 | (1 << 63)));
assert(swap64(swap64(-7))==-7);
assert(byte_at(0xabcd,1)==0xab);
assert(with_byte(-1,7,0)==0x00ffffffffffffff);
assert(signed8(255)==-1 && signed16(65535)==-1 && signed32(4294967295)==-1);
print 1;
