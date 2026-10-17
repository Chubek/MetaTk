// bits: bit manipulation on 64-bit words.
//
//     use bits;
//     print popcount(0xff), clz(1), rotl(1, 65), extract(0xabcd, 4, 8);
//
// Bit 0 is the least significant. `>>` in QScript is a logical shift; use
// sar() for an arithmetic (sign-propagating) shift. Shift counts must be in
// 0..63; rotl/rotr reduce theirs modulo 64.
fn popcount(x) {
    let c = 0;
    while (x != 0) { x = x & (x - 1); c++; }
    return c;
}
fn parity(x) { return popcount(x) & 1; }
// Leading / trailing zero counts; 64 for zero.
fn clz(x) {
    if (x == 0) return 64;
    let n = 0;
    while (x > 0) { x = x << 1; n++; }
    return n;
}
fn ctz(x) {
    if (x == 0) return 64;
    let n = 0;
    while ((x & 1) == 0) { x = x >> 1; n++; }
    return n;
}
fn bit_length(x) { return 64 - clz(x); }
fn rotl(x, n) {
    n = n & 63;
    if (n == 0) return x;
    return (x << n) | (x >> (64 - n));
}
fn rotr(x, n) {
    n = n & 63;
    if (n == 0) return x;
    return (x >> n) | (x << (64 - n));
}
fn sar(x, n) { return x >= 0 ? x >> n : ~(~x >> n); }
fn mask(width) { return width >= 64 ? -1 : (1 << width) - 1; }
fn bit(x, i) { return (x >> i) & 1; }
fn set_bit(x, i) { return x | (1 << i); }
fn clear_bit(x, i) { return x & ~(1 << i); }
fn toggle_bit(x, i) { return x ^ (1 << i); }
fn extract(x, offset, width) { return (x >> offset) & mask(width); }
fn insert(x, value, offset, width) {
    let m = mask(width) << offset;
    return (x & ~m) | ((value << offset) & m);
}
fn lowbit(x) { return x & -x; }
fn is_pow2(x) { return x > 0 && (x & (x - 1)) == 0; }
fn next_pow2(x) {
    assert x <= 4611686018427387904;  // 2**62
    let p = 1;
    while (p < x) p = p << 1;
    return p;
}
fn bswap16(x) { return ((x & 0xff) << 8) | ((x >> 8) & 0xff); }
fn bswap32(x) { return (bswap16(x) << 16) | bswap16(x >> 16); }
fn bswap64(x) { return (bswap32(x) << 32) | bswap32(x >> 32); }
fn reverse(x) {
    let r = 0;
    for (let i = 0; i < 64; i++) { r = (r << 1) | (x & 1); x = x >> 1; }
    return r;
}
