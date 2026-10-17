// Bit manipulation examples: population count and byte swap of 0x1234.
let x = 0x1234;
let count = 0;
let v = x;
while (v != 0) {
    count = count + (v & 1);
    v = v >> 1;
}
print count;
let hi = (x >> 8) & 0xff;
let lo = x & 0xff;
let swapped = (lo << 8) | hi;
print swapped;
return count;
