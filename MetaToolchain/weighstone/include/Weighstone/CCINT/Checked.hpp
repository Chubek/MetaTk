#pragma once
#include "BitVector.hpp"
namespace weighstone::ccint {
struct CheckedValue { BitVector value; bool unsigned_overflow; bool signed_overflow; };
inline CheckedValue checked_add(const BitVector& a, const BitVector& b) {
    return {a+b,a.unsigned_add_overflow(b),a.signed_add_overflow(b)};
}
inline CheckedValue checked_sub(const BitVector& a, const BitVector& b) {
    if (a.width()!=b.width()) throw std::invalid_argument("mismatched widths");
    const BigInt s = a.signed_value()-b.signed_value();
    return {a-b, a.unsigned_value()<b.unsigned_value(), s < -power_of_two(a.width()-1) || s >= power_of_two(a.width()-1)};
}
inline CheckedValue checked_mul(const BitVector& a, const BitVector& b) {
    if (a.width()!=b.width()) throw std::invalid_argument("mismatched widths");
    const BigInt s = a.signed_value()*b.signed_value();
    return {a*b,a.unsigned_value()*b.unsigned_value()>=power_of_two(a.width()),s < -power_of_two(a.width()-1) || s >= power_of_two(a.width()-1)};
}
}
