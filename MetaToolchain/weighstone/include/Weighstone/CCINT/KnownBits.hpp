#pragma once
#include "BitVector.hpp"
#include <stdexcept>
namespace weighstone::ccint {
class KnownBits {
    unsigned width_;
    BigInt zero_, one_;
    void check(const KnownBits& b) const { if(width_!=b.width_) throw std::invalid_argument("mismatched widths"); }
public:
    KnownBits(unsigned width, BigInt known_zero=0, BigInt known_one=0)
        : width_(width), zero_(0), one_(0) {
        // Materialize the value: a Boost expression would reference a temporary.
        const BigInt mask=BitVector(width,0).mask();
        zero_=known_zero&mask; one_=known_one&mask;
        if ((zero_&one_)!=0) throw std::invalid_argument("conflicting known bits");
    }
    static KnownBits constant(const BitVector& v) { return {v.width(), v.mask()^v.unsigned_value(), v.unsigned_value()}; }
    unsigned width() const noexcept { return width_; }
    const BigInt& known_zero() const noexcept { return zero_; }
    const BigInt& known_one() const noexcept { return one_; }
    bool is_constant() const { return (zero_|one_)==power_of_two(width_)-1; }
    BitVector constant_value() const { if(!is_constant()) throw std::logic_error("not constant"); return {width_,one_}; }
    KnownBits join(const KnownBits& b) const { check(b); return {width_,zero_&b.zero_,one_&b.one_}; }
    KnownBits bit_and(const KnownBits& b) const { check(b); return {width_,zero_|b.zero_,one_&b.one_}; }
    KnownBits bit_or(const KnownBits& b) const { check(b); return {width_,zero_&b.zero_,one_|b.one_}; }
    KnownBits bit_xor(const KnownBits& b) const {
        check(b);
        return {width_,(zero_&b.zero_)|(one_&b.one_),(one_&b.zero_)|(zero_&b.one_)};
    }
};
}
