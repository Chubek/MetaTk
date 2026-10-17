// ascii: character classification and conversion on ASCII codes.
//
//     use ascii;
//     print is_digit('7'), to_upper('q'), digit_value('9'), hex_value('F'), rot13('a');
//
// Characters are integers (QScript 'x' literals). Codes outside 0..127 are
// never letters, digits or punctuation.
fn is_digit(c) { return c >= '0' && c <= '9'; }
fn is_upper(c) { return c >= 'A' && c <= 'Z'; }
fn is_lower(c) { return c >= 'a' && c <= 'z'; }
fn is_alpha(c) { return is_upper(c) || is_lower(c); }
fn is_alnum(c) { return is_alpha(c) || is_digit(c); }
fn is_xdigit(c) { return is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }
fn is_space(c) { return c == ' ' || (c >= 9 && c <= 13); }
fn is_print(c) { return c >= 32 && c <= 126; }
fn is_cntrl(c) { return (c >= 0 && c < 32) || c == 127; }
fn is_punct(c) { return is_print(c) && !is_alnum(c) && c != ' '; }
fn is_vowel(c) { let l = to_lower(c); return l == 'a' || l == 'e' || l == 'i' || l == 'o' || l == 'u'; }
fn to_upper(c) { return is_lower(c) ? c - 32 : c; }
fn to_lower(c) { return is_upper(c) ? c + 32 : c; }
// Value of a decimal / hexadecimal / base-36 digit, or -1.
fn digit_value(c) { return is_digit(c) ? c - '0' : -1; }
fn hex_value(c) {
    if (is_digit(c)) return c - '0';
    let l = to_lower(c);
    return l >= 'a' && l <= 'f' ? l - 'a' + 10 : -1;
}
fn base36_value(c) {
    if (is_digit(c)) return c - '0';
    return is_alpha(c) ? to_lower(c) - 'a' + 10 : -1;
}
// The character for digit d in 0..35 ('0'..'9', 'a'..'z').
fn to_digit(d) { assert d >= 0 && d < 36; return d < 10 ? '0' + d : 'a' + d - 10; }
fn caesar(c, k) {
    k = k % 26;
    if (k < 0) k += 26;
    if (is_upper(c)) return 'A' + (c - 'A' + k) % 26;
    if (is_lower(c)) return 'a' + (c - 'a' + k) % 26;
    return c;
}
fn rot13(c) { return caesar(c, 13); }
