// hash: non-cryptographic hashing of integers and byte streams.
//
//     use hash;
//     let h = FNV_OFFSET;
//     h = fnv1a_byte(h, 'h'); h = fnv1a_byte(h, 'i');
//     print h, mix64(42), combine(hash_int(1), 2), bucket(h, 16);
//
// *_byte functions fold one byte (the low 8 bits) into a running state, so
// streams are hashed by threading the state through calls.
const FNV_OFFSET = -3750763034362895579;   // 0xcbf29ce484222325
const FNV_PRIME = 1099511628211;
const DJB2_INIT = 5381;
const _M32 = 0xffffffff;

fn fnv1a_byte(h, byte) { return (h ^ (byte & 255)) * FNV_PRIME; }
// FNV-1a of the 8 little-endian bytes of x.
fn fnv1a(x) {
    let h = FNV_OFFSET;
    for (let i = 0; i < 8; i++) { h = fnv1a_byte(h, x); x = x >> 8; }
    return h;
}
fn djb2_byte(h, byte) { return h * 33 + (byte & 255); }
// MurmurHash3 64-bit finalizer.
fn mix64(x) {
    x = x ^ (x >> 33);
    x = x * -49064778989728563;      // 0xff51afd7ed558ccd
    x = x ^ (x >> 33);
    x = x * -4265267296055464877;    // 0xc4ceb9fe1a85ec53
    return x ^ (x >> 33);
}
fn hash_int(x) { return mix64(x); }
fn combine(seed, v) {
    return seed ^ (mix64(v) + -7046029254386353131 + (seed << 6) + (seed >> 2));
}
fn hash2(a, b) { return combine(mix64(a), b); }
fn hash3(a, b, c) { return combine(hash2(a, b), c); }
// Jenkins one-at-a-time, 32-bit.
fn jenkins_byte(h, byte) {
    h = (h + (byte & 255)) & _M32;
    h = (h + (h << 10)) & _M32;
    return h ^ (h >> 6);
}
fn jenkins_final(h) {
    h = (h + (h << 3)) & _M32;
    h = h ^ (h >> 11);
    return (h + (h << 15)) & _M32;
}
// Map a hash to [0, n).
fn bucket(h, n) { assert n > 0; return (h >> 1) % n; }
