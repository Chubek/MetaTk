// range: closed integer intervals [lo, hi].
//
//     use range;
//     print contains(5, 1, 10), overlap(1, 5, 4, 9), wrap(13, 0, 9), sum(1, 100);  // 1 2 3 5050
use math;

fn contains(x, lo, hi) { return x >= lo && x <= hi; }
fn length(lo, hi) { return hi < lo ? 0 : hi - lo + 1; }
fn overlaps(a0, a1, b0, b1) { return a0 <= b1 && b0 <= a1; }
// Number of integers in both intervals.
fn overlap(a0, a1, b0, b1) { return length(max(a0, b0), min(a1, b1)); }
// x folded into [lo, hi] cyclically.
fn wrap(x, lo, hi) { return lo + mod_floor(x - lo, hi - lo + 1); }
// Linear map of x from [a0, a1] onto [b0, b1], truncating.
fn map(x, a0, a1, b0, b1) { return b0 + (x - a0) * (b1 - b0) / (a1 - a0); }
fn lerp(a, b, t, scale) { return a + (b - a) * t / scale; }
// Sum of the integers a..b.
fn sum(a, b) {
    if (b < a) return 0;
    let n = b - a + 1;
    return (n % 2 == 0) ? (n / 2) * (a + b) : n * ((a + b) / 2);
}
// Multiples of k in [a, b].
fn count_multiples(a, b, k) {
    assert k > 0;
    if (b < a) return 0;
    return div_floor(b, k) - div_floor(a - 1, k);
}
// Iterations of for (i = start; i < stop; i += step), step > 0.
fn steps(start, stop, step) {
    assert step > 0;
    return stop <= start ? 0 : (stop - start + step - 1) / step;
}
