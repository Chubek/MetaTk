#pragma once
#include "BigInt.hpp"
#include <optional>
namespace weighstone::ccint {
struct Bezout { BigInt gcd, x, y; };
inline Bezout extended_gcd(BigInt a, BigInt b) {
    BigInt old_r=a, r=b, old_s=1, s=0, old_t=0, t=1;
    while (r != 0) {
        BigInt q = old_r / r;
        BigInt nr=old_r-q*r, ns=old_s-q*s, nt=old_t-q*t;
        old_r=r; r=nr; old_s=s; s=ns; old_t=t; t=nt;
    }
    if (old_r < 0) return {-old_r,-old_s,-old_t};
    return {old_r,old_s,old_t};
}
inline std::optional<BigInt> mod_inverse(const BigInt& a, const BigInt& modulus) {
    if (modulus <= 0) throw std::domain_error("modulus must be positive");
    auto b = extended_gcd(a, modulus);
    if (b.gcd != 1) return std::nullopt;
    return euclidean_mod(b.x, modulus);
}
struct CongruenceSolution { BigInt residue, modulus; };
inline std::optional<CongruenceSolution> chinese_remainder(
    const BigInt& r1, const BigInt& m1, const BigInt& r2, const BigInt& m2) {
    if (m1 <= 0 || m2 <= 0) throw std::domain_error("modulus must be positive");
    const auto bez = extended_gcd(m1,m2);
    if ((r2-r1) % bez.gcd != 0) return std::nullopt;
    const BigInt m2r=m2/bez.gcd;
    const BigInt k=euclidean_mod(((r2-r1)/bez.gcd)*bez.x,m2r);
    const BigInt m=m1*m2r;
    return CongruenceSolution{euclidean_mod(r1+m1*k,m),m};
}
inline bool divides(const BigInt& a, const BigInt& b) { return a==0 ? b==0 : b%a==0; }
}
