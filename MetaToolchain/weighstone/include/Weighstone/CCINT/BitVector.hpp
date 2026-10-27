#pragma once
#include "BigInt.hpp"
#include <cstdint>
#include <stdexcept>
namespace weighstone::ccint {
class BitVector {
    unsigned bits_;
    BigInt val_;
    static void valid(unsigned bits) {
        if (bits == 0 || bits > 1048576) throw std::invalid_argument("bit-width outside 1..1048576");
    }
    void compatible(const BitVector& other) const {
        if (bits_ != other.bits_) throw std::invalid_argument("mismatched bit-widths");
    }
public:
    BitVector(unsigned bits, BigInt value) : bits_(bits), val_(0) {
        valid(bits); val_ = euclidean_mod(value,power_of_two(bits));
    }
    unsigned width() const noexcept { return bits_; }
    const BigInt& unsigned_value() const noexcept { return val_; }
    BigInt signed_value() const { return sign_bit() ? val_-power_of_two(bits_) : val_; }
    BigInt mask() const { return power_of_two(bits_)-1; }
    bool sign_bit() const { return boost::multiprecision::bit_test(val_,bits_-1); }
    bool unsigned_add_overflow(const BitVector& b) const { compatible(b); return val_+b.val_ >= power_of_two(bits_); }
    bool signed_add_overflow(const BitVector& b) const {
        compatible(b); const BigInt s = signed_value()+b.signed_value();
        return s < -power_of_two(bits_-1) || s >= power_of_two(bits_-1);
    }
    friend BitVector operator+(const BitVector& a,const BitVector& b) { a.compatible(b); return {a.bits_,a.val_+b.val_}; }
    friend BitVector operator-(const BitVector& a,const BitVector& b) { a.compatible(b); return {a.bits_,a.val_-b.val_}; }
    friend BitVector operator*(const BitVector& a,const BitVector& b) { a.compatible(b); return {a.bits_,a.val_*b.val_}; }
    friend BitVector operator&(const BitVector& a,const BitVector& b) { a.compatible(b); return {a.bits_,a.val_&b.val_}; }
    friend BitVector operator|(const BitVector& a,const BitVector& b) { a.compatible(b); return {a.bits_,a.val_|b.val_}; }
    friend BitVector operator^(const BitVector& a,const BitVector& b) { a.compatible(b); return {a.bits_,a.val_^b.val_}; }
    BitVector bit_not() const { return {bits_,mask()^val_}; }
    BitVector shl(unsigned shift) const { return {bits_,shift >= bits_ ? BigInt(0) : BigInt(val_ << shift)}; }
    BitVector lshr(unsigned shift) const { return {bits_,shift >= bits_ ? BigInt(0) : BigInt(val_ >> shift)}; }
    BitVector ashr(unsigned shift) const { return {bits_,signed_value() >> (shift >= bits_ ? bits_ : shift)}; }
    BitVector trunc(unsigned width) const { if (width > bits_) throw std::invalid_argument("trunc widens"); return {width,val_}; }
    BitVector zext(unsigned width) const { if (width < bits_) throw std::invalid_argument("zext narrows"); return {width,val_}; }
    BitVector sext(unsigned width) const { if (width < bits_) throw std::invalid_argument("sext narrows"); return {width,signed_value()}; }
    BitVector rol(unsigned s) const {
        s %= bits_; return {bits_, (val_ << s) | (val_ >> (bits_-s))};
    }
    BitVector ror(unsigned s) const {
        s %= bits_; return {bits_, (val_ >> s) | (val_ << (bits_-s))};
    }
    friend bool operator==(const BitVector& a,const BitVector& b) { return a.bits_==b.bits_ && a.val_==b.val_; }
};
}
