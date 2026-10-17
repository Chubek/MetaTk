# Saturating signed arithmetic, suitable for sizes and counters.
const MIN=-9223372036854775808;
const MAX=9223372036854775807;
fn saturating_add(a,b) {
    if (b>0 && a>MAX-b) return MAX;
    if (b<0 && a<MIN-b) return MIN;
    return a+b;
}
fn saturating_sub(a,b) {
    if (b>0 && a<MIN+b) return MIN;
    if (b<0 && a>MAX+b) return MAX;
    return a-b;
}
fn saturating_abs(x) { return x==MIN ? MAX : x<0 ? -x : x; }
# Overflow-free midpoint, rounded toward the first argument.
fn midpoint(a,b) {
    let common=(a & b);
    let differing=(a ^ b);
    # Logical right shift needs explicit restoration of its sign bit.
    let half=(differing >> 1) | (differing & MIN);
    return common+half+((a>b && (differing & 1)) ? 1 : 0);
}
fn is_power_of_two(x) { return x>0 && (x & (x-1))==0; }
fn align_up(value, alignment) {
    assert(value>=0 && is_power_of_two(alignment));
    assert(value<=MAX-(alignment-1));
    return (value+alignment-1) & -alignment;
}
