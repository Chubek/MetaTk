// random: deterministic pseudo-random numbers with explicit state.
//
//     use random;
//     let s = seed(2024);
//     for (let i = 0; i < 3; i++) { s = step(s); print range(s, 1, 6); }
//
// QScript has no global variables, so generators are pure functions: step()
// advances a state, and value()/below()/range() derive outputs from a state
// without changing it. The default generator is SplitMix64; xorshift and
// lcg are provided as raw step functions. Not for cryptography.
const GOLDEN = -7046029254386353131;     // 0x9E3779B97F4A7C15

// The SplitMix64 output function (also a good 64-bit integer hash).
fn mix(z) {
    z = (z ^ (z >> 30)) * -4658895280553007687;   // 0xBF58476D1CE4E5B9
    z = (z ^ (z >> 27)) * -7723592293110705685;   // 0x94D049BB133111EB
    return z ^ (z >> 31);
}
fn seed(s) { return mix(s + GOLDEN); }
fn step(state) { return state + GOLDEN; }
fn value(state) { return mix(state); }
// Uniform in [0, n) for n > 0 (tiny modulo bias for huge n).
fn below(state, n) {
    assert n > 0;
    return (mix(state) >> 1) % n;
}
fn range(state, lo, hi) { assert lo <= hi; return lo + below(state, hi - lo + 1); }
fn coin(state) { return mix(state) >> 63; }
// Q16.16 fraction in [0, 1).
fn fraction(state) { return mix(state) >> 48; }
// Marsaglia xorshift64 (13, 7, 17); state must be non-zero.
fn xorshift(x) {
    x = x ^ (x << 13);
    x = x ^ (x >> 7);
    return x ^ (x << 17);
}
// Knuth's MMIX linear congruential generator.
fn lcg(x) { return x * 6364136223846793005 + 1442695040888963407; }
