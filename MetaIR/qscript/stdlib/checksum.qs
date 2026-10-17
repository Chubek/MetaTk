// checksum: error-detecting codes over byte streams and digit strings.
//
//     use checksum;
//     let c = CRC32_INIT;
//     c = crc32_byte(c, '1'); c = crc32_byte(c, '2'); c = crc32_byte(c, '3');
//     print crc32_final(c), luhn_valid(79927398713), luhn_digit(7992739871);
//
// Streams are folded one byte at a time, threading the state.
const CRC32_INIT = 0xffffffff;
const ADLER32_INIT = 1;

// CRC-32 (IEEE 802.3, reflected, as used by zip and PNG).
fn crc32_byte(crc, byte) {
    crc = crc ^ (byte & 255);
    for (let k = 0; k < 8; k++) crc = (crc & 1) ? (crc >> 1) ^ 0xEDB88320 : crc >> 1;
    return crc;
}
fn crc32_final(crc) { return crc ^ 0xffffffff; }
// CRC-8 (polynomial 0x07, initial value 0).
fn crc8_byte(crc, byte) {
    crc = (crc ^ byte) & 255;
    for (let k = 0; k < 8; k++) crc = (crc & 0x80) ? ((crc << 1) ^ 0x07) & 255 : (crc << 1) & 255;
    return crc;
}
// Adler-32: state is (b << 16) | a.
fn adler32_byte(state, byte) {
    let a = state & 0xffff;
    let b = (state >> 16) & 0xffff;
    a = (a + (byte & 255)) % 65521;
    b = (b + a) % 65521;
    return (b << 16) | a;
}
// Fletcher-16: state is (sum2 << 8) | sum1, start at 0.
fn fletcher16_byte(state, byte) {
    let s1 = state & 255;
    let s2 = (state >> 8) & 255;
    s1 = (s1 + (byte & 255)) % 255;
    s2 = (s2 + s1) % 255;
    return (s2 << 8) | s1;
}
fn _luhn_sum(n) {
    let sum = 0;
    let double = 0;
    while (n > 0) {
        let d = n % 10;
        if (double) { d *= 2; if (d > 9) d -= 9; }
        sum += d;
        double = !double;
        n = n / 10;
    }
    return sum;
}
// Luhn (credit-card) check over the decimal digits of n.
fn luhn_valid(n) { return n > 0 && _luhn_sum(n) % 10 == 0; }
// The digit to append to n to make it Luhn-valid.
fn luhn_digit(n) { return (10 - _luhn_sum(n * 10) % 10) % 10; }
// ISO 7064 mod 97-10 (IBAN style): valid when n % 97 == 1.
fn mod97_valid(n) { return n % 97 == 1; }
