// pack: packing several small integers into one 64-bit word.
//
//     use pack;
//     let p = pack32(-5, 7);
//     print hi32(p), lo32(p), byte(0x1234, 1), sign_extend(0xff, 8);   // -5 7 18 -1
//
// QScript has no arrays or structs; packing is how libraries return or
// store several values at once (see date::civil, color::rgb).
fn zero_extend(x, bits) { return bits >= 64 ? x : x & ((1 << bits) - 1); }
fn sign_extend(x, bits) {
    if (bits >= 64) return x;
    let m = 1 << (bits - 1);
    x = x & ((1 << bits) - 1);
    return (x ^ m) - m;
}
fn pack32(hi, lo) { return (hi << 32) | (lo & 0xffffffff); }
fn hi32(x) { return sign_extend(x >> 32, 32); }
fn lo32(x) { return sign_extend(x, 32); }
fn u32(x) { return x & 0xffffffff; }
fn i32(x) { return sign_extend(x, 32); }
fn u16(x) { return x & 0xffff; }
fn i16(x) { return sign_extend(x, 16); }
fn u8(x) { return x & 0xff; }
fn i8(x) { return sign_extend(x, 8); }
fn byte(x, i) { return (x >> (8 * i)) & 255; }
fn with_byte(x, i, b) { return (x & ~(255 << (8 * i))) | ((b & 255) << (8 * i)); }
fn pack16(a, b, c, d) { return ((a & 0xffff) << 48) | ((b & 0xffff) << 32) | ((c & 0xffff) << 16) | (d & 0xffff); }
// Field i (0 = lowest) of four unsigned 16-bit fields.
fn field16(x, i) { return (x >> (16 * i)) & 0xffff; }
fn pack_bytes(b7, b6, b5, b4, b3, b2, b1, b0) {
    let r = 0;
    r = (r << 8) | (b7 & 255); r = (r << 8) | (b6 & 255);
    r = (r << 8) | (b5 & 255); r = (r << 8) | (b4 & 255);
    r = (r << 8) | (b3 & 255); r = (r << 8) | (b2 & 255);
    r = (r << 8) | (b1 & 255);
    return (r << 8) | (b0 & 255);
}
