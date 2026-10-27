// Internal fixed-width adapter. Weighstone intervals are mathematical integers;
// every arithmetic result is mapped back into MetaIL's signed-64 domain.
#pragma once
#include <Weighstone/CCINT/KnownBits.hpp>
#include <Weighstone/CCINT/Interval.hpp>
#include <Weighstone/CCINT/Constraints.hpp>
#include <Weighstone/CCINT/Congruence.hpp>
#include <bit>
#include <cstdint>
#include <limits>
#include <string_view>

namespace metail::weighstone_detail {
using namespace weighstone::ccint;
struct Facts {
    std::int64_t lo = INT64_MIN, hi = INT64_MAX;
    std::int64_t zero = 0, one = 0; // bit patterns, transported as Lua integers
    bool total = true;             // this operator cannot trap (not subtree purity)
};
inline BigInt bits(std::int64_t n) { return BitVector(64, n).unsigned_value(); }
inline std::int64_t signed_bits(const BigInt& n) {
    return BitVector(64, n).signed_value().convert_to<std::int64_t>();
}
inline Interval full() { return {BigInt(INT64_MIN), BigInt(INT64_MAX)}; }
inline Interval wrap(const Interval& r) {
    const BigInt modulus = power_of_two(64), bias = power_of_two(63);
    const BigInt a = floor_div(*r.lower() + bias, modulus);
    const BigInt b = floor_div(*r.upper() + bias, modulus);
    if (a != b) return full();
    return {*r.lower() - a * modulus, *r.upper() - a * modulus};
}
inline KnownBits range_bits(const Interval& r) {
    if ((*r.lower() < 0) != (*r.upper() < 0)) return KnownBits(64);
    const BigInt lo = BitVector(64, *r.lower()).unsigned_value();
    const BigInt hi = BitVector(64, *r.upper()).unsigned_value();
    const BigInt diff = lo ^ hi;
    const BigInt varying = diff == 0 ? BigInt(0) : power_of_two(boost::multiprecision::msb(diff) + 1) - 1;
    const BigInt fixed = (power_of_two(64) - 1) ^ varying;
    return {64, fixed & ~lo, fixed & lo};
}
inline Interval bit_range(const KnownBits& k) {
    const BigInt sign = power_of_two(63), low = sign - 1;
    BigInt lo = k.known_one() & low, hi = low ^ (k.known_zero() & low);
    if ((k.known_zero() & sign) == 0) lo -= sign;
    if ((k.known_one() & sign) != 0) hi -= sign;
    return {lo, hi};
}
inline Facts pack(Interval r, KnownBits k, bool total = true) {
    // A small reduced product: interval bounds imply high bits, and known bits
    // imply signed bounds. Never infer a fact from an empty intersection.
    for (int i = 0; i < 2; ++i) {
        r = r.meet(bit_range(k));
        if (r.empty()) throw std::invalid_argument("inconsistent integer facts");
        const auto extra = range_bits(r);
        k = KnownBits(64, k.known_zero() | extra.known_zero(), k.known_one() | extra.known_one());
    }
    return {r.lower()->convert_to<std::int64_t>(), r.upper()->convert_to<std::int64_t>(),
            signed_bits(k.known_zero()), signed_bits(k.known_one()), total};
}
inline Congruence low_congruence(const KnownBits& k) {
    const BigInt known = k.known_zero() | k.known_one();
    const unsigned n = std::countr_one(known.convert_to<std::uint64_t>());
    return {k.known_one(), power_of_two(n)};
}
inline KnownBits congruence_bits(const Congruence& c) {
    // Reduction modulo 2^64 preserves precisely gcd(modulus, 2^64).
    const BigInt modulus = gcd(c.modulus(), power_of_two(64));
    const BigInt mask = modulus - 1, one = c.residue() & mask;
    return {64, mask ^ one, one};
}
inline Facts transfer(std::string_view op, Facts a, Facts b = {}) {
    if (a.lo > a.hi || b.lo > b.hi) throw std::invalid_argument("reversed integer range");
    Interval x(a.lo, a.hi), y(b.lo, b.hi), r = full();
    KnownBits ka(64, bits(a.zero), bits(a.one)), kb(64, bits(b.zero), bits(b.one)), k(64);
    // Validate externally supplied facts too.
    a = pack(x, ka); b = pack(y, kb);
    x = Interval(a.lo, a.hi); y = Interval(b.lo, b.hi);
    ka = KnownBits(64, bits(a.zero), bits(a.one));
    kb = KnownBits(64, bits(b.zero), bits(b.one));
    bool total = true;
    if (op == "identity") return a;
    if (op == "join") { r = x.join(y); k = ka.join(kb); }
    else if (op == "band") k = ka.bit_and(kb);
    else if (op == "bor") k = ka.bit_or(kb);
    else if (op == "bxor") k = ka.bit_xor(kb);
    else if (op == "bnot") { k = KnownBits(64, ka.known_one(), ka.known_zero()); r = x.negate().sub(Interval::point(1)); }
    else if (op == "add" || op == "sub" || op == "mul" || op == "neg") {
        if (op == "add") r = wrap(x.add(y));
        else if (op == "sub") r = wrap(x.sub(y));
        else if (op == "mul") r = wrap(x.mul(y));
        else r = wrap(x.negate());
        const auto ca = low_congruence(ka), cb = low_congruence(kb);
        if (op == "add") k = congruence_bits(ca.add(cb));
        else if (op == "sub") k = congruence_bits(ca.sub(cb));
        else if (op == "neg") k = congruence_bits(Congruence::point(0).sub(ca));
        else {
            // (ra + ma*i)*(rb + mb*j): all variable terms are multiples
            // of gcd(ma*mb, ra*mb, rb*ma), before fixed-width reduction.
            const BigInt modulus = gcd(ca.modulus() * cb.modulus(),
                gcd(ca.residue() * cb.modulus(), cb.residue() * ca.modulus()));
            k = congruence_bits(Congruence(ca.residue() * cb.residue(), modulus));
        }
    } else if (op == "shl" || op == "shr") {
        total = b.lo >= 0 && b.hi < 64;
        if (total && b.lo == b.hi) {
            const auto n = static_cast<unsigned>(b.lo);
            if (op == "shl") {
                k = KnownBits(64, (ka.known_zero() << n) | (power_of_two(n) - 1), ka.known_one() << n);
                r = wrap(x.scale(power_of_two(n)));
            } else {
                const BigInt high = ((power_of_two(n) - 1) << (64 - n));
                BigInt zero = ka.known_zero() >> n, one = ka.known_one() >> n;
                if (a.lo >= 0) zero |= high;
                if (a.hi < 0) one |= high;
                k = KnownBits(64, zero, one);
                r = Interval(BigInt(a.lo) >> n, BigInt(a.hi) >> n);
            }
        }
    } else if (op == "div" || op == "mod") {
        total = !y.contains(0) && !(x.contains(BigInt(INT64_MIN)) && y.contains(-1));
        if (total && op == "div") {
            const BigInt v[] = {BigInt(a.lo) / b.lo, BigInt(a.lo) / b.hi,
                               BigInt(a.hi) / b.lo, BigInt(a.hi) / b.hi};
            r = Interval(*std::min_element(v, v + 4), *std::max_element(v, v + 4));
        } else if (total) {
            const BigInt bound = std::max(abs(BigInt(b.lo)), abs(BigInt(b.hi))) - 1;
            const BigInt lo = std::max(BigInt(a.lo), BigInt(-bound));
            const BigInt hi = std::min(BigInt(a.hi), bound);
            r = Interval(a.lo >= 0 ? BigInt(0) : std::min(lo, BigInt(0)),
                         a.hi <= 0 ? BigInt(0) : std::max(hi, BigInt(0)));
            if (b.lo == b.hi && BigInt(a.lo) / b.lo == BigInt(a.hi) / b.lo)
                r = x.sub(Interval::point((BigInt(a.lo) / b.lo) * b.lo));
            if (b.lo == b.hi) {
                const BigInt d = abs(BigInt(b.lo));
                const auto ca = low_congruence(ka);
                if (ca.modulus() % d == 0 && ca.residue() % d == 0) r = Interval::point(0);
                else if (a.lo >= 0 && (d & (d - 1)) == 0)
                    k = ka.bit_and(KnownBits::constant(BitVector(64, d - 1)));
            }
        }
    } else {
        // A boolean proof is tri-state: true everywhere, false everywhere, or
        // unknown. An overlapping interval is never treated as a proof.
        Verdict verdict;
        const bool disjoint = a.hi < b.lo || b.hi < a.lo ||
            ((ka.known_one() & kb.known_zero()) | (kb.known_one() & ka.known_zero())) != 0;
        if (op == "eq" || op == "ne" || op == "lt" || op == "le" || op == "gt" || op == "ge") {
            // The difference here is mathematical, not an IR subtraction. This
            // avoids signed overflow in ordering proofs at MIN/MAX.
            const Relation relation = op == "lt" || op == "le" ? Relation::less_equal :
                op == "gt" || op == "ge" ? Relation::greater_equal : Relation::equal;
            const BigInt rhs = op == "lt" ? -1 : op == "gt" ? 1 : 0;
            verdict = check({LinearExpr::variable("difference"), relation, rhs}, {{"difference", x.sub(y)}});
            if ((op == "eq" || op == "ne") && disjoint) verdict = {Decision::disproven, "conflicting bits or bounds"};
            if (op == "ne") {
                if (verdict.is_proven()) verdict.decision = Decision::disproven;
                else if (verdict.is_disproven()) verdict.decision = Decision::proven;
            }
        } else if (op == "lnot" || op == "land" || op == "lor") {
            const bool az = a.lo == 0 && a.hi == 0, bz = b.lo == 0 && b.hi == 0;
            const bool an = !x.contains(0) || ka.known_one() != 0;
            const bool bn = !y.contains(0) || kb.known_one() != 0;
            if (op == "lnot") {
                if (az) verdict.decision = Decision::proven; else if (an) verdict.decision = Decision::disproven;
            } else if (op == "land") {
                if (az || bz) verdict.decision = Decision::disproven; else if (an && bn) verdict.decision = Decision::proven;
            } else {
                if (an || bn) verdict.decision = Decision::proven; else if (az && bz) verdict.decision = Decision::disproven;
            }
        } else throw std::invalid_argument("unknown Weighstone transfer operator");
        r = verdict.is_proven() ? Interval::point(1) : verdict.is_disproven() ? Interval::point(0) : Interval(0, 1);
    }
    return pack(r, k, total);
}
} // namespace metail::weighstone_detail
