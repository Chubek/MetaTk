# Byte order and signed extension for packed integer data.
fn swap16(x) { return ((x & 255) << 8) | ((x >> 8) & 255); }
fn swap32(x) { return (swap16(x) << 16) | swap16(x >> 16); }
fn swap64(x) { return (swap32(x) << 32) | swap32(x >> 32); }
fn byte_at(x, index) { assert(index>=0 && index<8); return (x >> (8*index)) & 255; }
fn with_byte(x, index, value) {
    assert(index>=0 && index<8 && value>=0 && value<=255);
    let shift=8*index;
    return (x & ~(255 << shift)) | (value << shift);
}
fn signed8(x) { x=x & 255; return x>=128 ? x-256 : x; }
fn signed16(x) { x=x & 65535; return x>=32768 ? x-65536 : x; }
fn signed32(x) { x=x & 4294967295; return x>=2147483648 ? x-4294967296 : x; }
