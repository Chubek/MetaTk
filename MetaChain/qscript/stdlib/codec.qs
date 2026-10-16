// codec: integer encodings.
//
//     use codec;
//     print zigzag(-3), unzigzag(5), gray(6), ungray(5), bcd(1234) == 0x1234, varint_size(300);
//     // 5 -3 5 6 1 2
fn zigzag(x) { return (x << 1) ^ (x < 0 ? -1 : 0); }
fn unzigzag(z) { return (z >> 1) ^ -(z & 1); }
fn gray(x) { return x ^ (x >> 1); }
fn ungray(g) {
    let x = g;
    while (g != 0) { g = g >> 1; x = x ^ g; }
    return x;
}
// Binary-coded decimal (n >= 0, up to 16 digits).
fn bcd(n) {
    assert n >= 0;
    let r = 0; let shift = 0;
    while (n > 0) { r = r | ((n % 10) << shift); n = n / 10; shift += 4; }
    return r;
}
fn unbcd(b) {
    let r = 0; let scale = 1;
    while (b != 0) { r += (b & 15) * scale; b = b >> 4; scale *= 10; }
    return r;
}
// Bytes in the unsigned LEB128 encoding of x (treated as unsigned).
fn varint_size(x) {
    let n = 1;
    while ((x >> 7) != 0) { x = x >> 7; n++; }
    return n;
}
// Interleave the low 32 bits of x and y (Z-order / Morton code).
fn _spread(v) {
    let r = 0;
    for (let i = 0; i < 32; i++) r = r | (((v >> i) & 1) << (2 * i));
    return r;
}
fn _compact(v) {
    let r = 0;
    for (let i = 0; i < 32; i++) r = r | (((v >> (2 * i)) & 1) << i);
    return r;
}
fn morton(x, y) { return _spread(x) | (_spread(y) << 1); }
fn morton_x(m) { return _compact(m); }
fn morton_y(m) { return _compact(m >> 1); }
