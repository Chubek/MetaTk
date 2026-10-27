#pragma once
#include <boost/multiprecision/cpp_int.hpp>
#include <limits>
#include <stdexcept>
#include <string>

namespace weighstone::ccint {
using BigInt = boost::multiprecision::cpp_int;
inline BigInt abs(BigInt n) { return n < 0 ? -n : n; }
inline BigInt gcd(BigInt a, BigInt b) {
    a = abs(a); b = abs(b);
    while (b != 0) { BigInt r = a % b; a = b; b = r; }
    return a;
}
inline BigInt lcm(const BigInt& a, const BigInt& b) {
    return a == 0 || b == 0 ? BigInt(0) : abs((a / gcd(a,b))*b);
}
inline BigInt floor_div(const BigInt& a, const BigInt& b) {
    if (b == 0) throw std::domain_error("division by zero");
    BigInt q = a/b, r = a%b;
    if (r != 0 && ((r < 0) != (b < 0))) --q;
    return q;
}
inline BigInt ceil_div(const BigInt& a, const BigInt& b) {
    return -floor_div(-a,b);
}
inline BigInt euclidean_mod(const BigInt& a, const BigInt& m) {
    if (m <= 0) throw std::domain_error("modulus must be positive");
    BigInt r = a % m;
    if (r < 0) r += m;
    return r;
}
inline BigInt power_of_two(unsigned bits) {
    BigInt result=0;
    boost::multiprecision::bit_set(result,bits);
    return result;
}
}
