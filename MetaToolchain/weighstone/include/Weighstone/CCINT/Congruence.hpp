#pragma once
#include "NumberTheory.hpp"
#include <optional>
namespace weighstone::ccint {
// Modulus zero denotes an exact singleton; modulus one denotes top.
class Congruence {
    BigInt residue_=0, modulus_=1;
public:
    Congruence(BigInt residue,BigInt modulus) : residue_(std::move(residue)),modulus_(std::move(modulus)) {
        if (modulus_<0) throw std::invalid_argument("negative modulus");
        if (modulus_!=0) residue_=euclidean_mod(residue_,modulus_);
    }
    static Congruence top() { return {0,1}; }
    static Congruence point(BigInt v) { return {std::move(v),0}; }
    const BigInt& residue() const { return residue_; }
    const BigInt& modulus() const { return modulus_; }
    bool contains(const BigInt& x) const { return modulus_==0 ? x==residue_ : (x-residue_)%modulus_==0; }
    Congruence add(const Congruence& b) const {
        return {residue_+b.residue_,gcd(modulus_,b.modulus_)};
    }
    Congruence sub(const Congruence& b) const {
        return {residue_-b.residue_,gcd(modulus_,b.modulus_)};
    }
    Congruence join(const Congruence& b) const {
        return {residue_,gcd(gcd(modulus_,b.modulus_),abs(residue_-b.residue_))};
    }
    std::optional<Congruence> meet(const Congruence& b) const {
        if(modulus_==0) return b.contains(residue_) ? std::optional<Congruence>(*this) : std::nullopt;
        if(b.modulus_==0) return contains(b.residue_) ? std::optional<Congruence>(b) : std::nullopt;
        auto r=chinese_remainder(residue_,modulus_,b.residue_,b.modulus_);
        if(!r) return std::nullopt;
        return Congruence{r->residue,r->modulus};
    }
};
}
