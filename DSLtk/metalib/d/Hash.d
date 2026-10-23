/**
 * metalib/d/Hash.d — MetaML host module `Hash` (D dialect): hashing and digests.
 *
 * Primitives (identical names, types and behaviour in metalib/cpp/Hash.h):
 *   Hash.fnv1a, djb2, value, combine, crc32, sha256, hex
 */
module metalib.d.Hash;

import MetaML;
import metalib.d.Support;


private string hex64(ulong v)
{
    enum d = "0123456789abcdef";
    char[16] result;
    for (int i = 15; i >= 0; --i)
    {
        result[i] = d[v & 15];
        v >>= 4;
    }
    return result.idup;
}

private uint crc32(string s)
{
    uint crc = 0xFFFFFFFFu;
    foreach (char ch; s)
    {
        crc ^= cast(ubyte) ch;
        foreach (k; 0 .. 8) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return crc ^ 0xFFFFFFFFu;
}

private string sha256(string msg)
{
    static immutable uint[64] K = [
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4,
        0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe,
        0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f,
        0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
        0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc,
        0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
        0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116,
        0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7,
        0xc67178f2];
    uint[8] h = [0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19];
    ubyte[] data = cast(ubyte[]) msg.dup;
    ulong bits = cast(ulong) msg.length * 8;
    data ~= 0x80;
    while (data.length % 64 != 56) data ~= 0;
    for (int i = 7; i >= 0; --i) data ~= cast(ubyte)((bits >> (i * 8)) & 0xFF);
    static uint rotr(uint x, int n) { return (x >> n) | (x << (32 - n)); }
    for (size_t off = 0; off < data.length; off += 64)
    {
        uint[64] w;
        foreach (i; 0 .. 16)
        {
            size_t p = off + i * 4;
            w[i] = (cast(uint) data[p] << 24) | (cast(uint) data[p + 1] << 16) | (cast(uint) data[p + 2] << 8)
                | cast(uint) data[p + 3];
        }
        foreach (i; 16 .. 64)
        {
            uint s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            uint s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        uint a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
        foreach (i; 0 .. 64)
        {
            uint S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            uint ch = (e & f) ^ (~e & g);
            uint t1 = hh + S1 + ch + K[i] + w[i];
            uint S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            uint mj = (a & b) ^ (a & c) ^ (b & c);
            uint t2 = S0 + mj;
            hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }
    string result;
    foreach (x; h) result ~= hex64(x)[8 .. $];
    return result;
}

void installHash(Session s)
{
    prim(s, "Hash.fnv1a", "String -> Int", 1, (C c, V a) => integer(fnv1a(S(a[0]))));
    prim(s, "Hash.djb2", "String -> Int", 1, delegate Value(C c, V a) {
        ulong h = 5381;
        foreach (char ch; S(a[0])) h = h * 33 + cast(ubyte) ch;
        return integer(cast(long) h);
    });
    prim(s, "Hash.value", "'a -> Int", 1, (C c, V a) => integer(fnv1a(show_value(a[0]))));
    prim(s, "Hash.combine", "Int -> Int -> Int", 2, delegate Value(C c, V a) {
        ulong h1 = cast(ulong) a[0].i, h2 = cast(ulong) a[1].i;
        return integer(cast(long)(h1 ^ (h2 + 0x9e3779b97f4a7c15UL + (h1 << 6) + (h1 >> 2))));
    });
    prim(s, "Hash.crc32", "String -> Int", 1, (C c, V a) => integer(cast(long) crc32(S(a[0]))));
    prim(s, "Hash.sha256", "String -> String", 1, (C c, V a) => str(sha256(S(a[0]))));
    prim(s, "Hash.hex", "Int -> String", 1, (C c, V a) => str(hex64(cast(ulong) a[0].i)));
}

