// round: rounding division and rounding to multiples.
//
//     use round;
//     print round_div(7, 2), round_half_even(5, 2), round_to(1234, 100), percent(1, 3);  // 4 2 1200 33
//
// Divisors and multiples must be non-zero. "half" cases round away from
// zero unless the name says otherwise.
use math;

fn trunc_div(a, b) { return a / b; }
fn round_div(a, b) {
    let q = a / b;
    let r = a % b;
    if (2 * abs(r) >= abs(b)) q += (a < 0) == (b < 0) ? 1 : -1;
    return q;
}
fn round_half_even(a, b) {
    let q = a / b;
    let r = abs(a % b);
    let twice = 2 * r;
    let bb = abs(b);
    if (twice > bb || (twice == bb && (q & 1) != 0)) q += (a < 0) == (b < 0) ? 1 : -1;
    return q;
}
fn floor_to(x, m) { return div_floor(x, m) * m; }
fn ceil_to(x, m) { return div_ceil(x, m) * m; }
fn round_to(x, m) { return round_div(x, m) * m; }
fn trunc_to(x, m) { return x / m * m; }
// part as a rounded percentage / per-mille of whole.
fn percent(part, whole) { return round_div(part * 100, whole); }
fn permille(part, whole) { return round_div(part * 1000, whole); }
// x * num / den, rounded, with the multiplication done first.
fn scale(x, num, den) { return round_div(x * num, den); }
