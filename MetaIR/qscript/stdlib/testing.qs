// testing: assertion helpers for QScript test programs.
//
//     use testing, prime;
//     equal(2 + 2, 4);
//     between(next_prime(10), 11, 13);
//     let passed = check(0, is_prime(9) == 0, 1);   // counts passes, prints failure code 1
//
// equal/not_equal/less/between/is_true/is_false print the offending values
// and then trap with "assertion failed" (the failing line is reported).
// check() is non-fatal: it prints `code` on failure and returns passed + 1
// on success, so a program can count results.
fn equal(actual, expected) {
    if (actual != expected) { print actual, expected; assert false; }
    return 1;
}
fn not_equal(a, b) {
    if (a == b) { print a, b; assert false; }
    return 1;
}
fn less(a, b) {
    if (!(a < b)) { print a, b; assert false; }
    return 1;
}
fn between(x, lo, hi) {
    if (x < lo || x > hi) { print x, lo, hi; assert false; }
    return 1;
}
fn is_true(x) { if (!x) { print x; assert false; } return 1; }
fn is_false(x) { if (x) { print x; assert false; } return 1; }
fn check(passed, condition, code) {
    if (condition) return passed + 1;
    print code;
    return passed;
}
