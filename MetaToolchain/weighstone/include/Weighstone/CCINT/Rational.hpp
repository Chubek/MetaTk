#pragma once
#include "BigInt.hpp"
#include <compare>
#include <stdexcept>
#include <string>
namespace weighstone::ccint {
class Rational {
    BigInt n_ = 0, d_ = 1;
    void normalize() {
        if (d_ == 0) throw std::domain_error("rational denominator zero");
        if (d_ < 0) { n_ = -n_; d_ = -d_; }
        const BigInt g = gcd(n_, d_); n_ /= g; d_ /= g;
    }
public:
    Rational() = default;
    Rational(BigInt n) : n_(std::move(n)) {}
    Rational(BigInt n, BigInt d) : n_(std::move(n)), d_(std::move(d)) { normalize(); }
    const BigInt& num() const noexcept { return n_; }
    const BigInt& den() const noexcept { return d_; }
    Rational operator-() const { return {-n_, d_}; }
    friend Rational operator+(const Rational& a,const Rational& b) { return {a.n_*b.d_+b.n_*a.d_,a.d_*b.d_}; }
    friend Rational operator-(const Rational& a,const Rational& b) { return a+(-b); }
    friend Rational operator*(const Rational& a,const Rational& b) { return {a.n_*b.n_,a.d_*b.d_}; }
    friend Rational operator/(const Rational& a,const Rational& b) { return {a.n_*b.d_,a.d_*b.n_}; }
    friend bool operator==(const Rational& a,const Rational& b) { return a.n_==b.n_ && a.d_==b.d_; }
    friend std::strong_ordering operator<=>(const Rational& a,const Rational& b) {
        BigInt cmp = a.n_*b.d_ - b.n_*a.d_;
        return cmp < 0 ? std::strong_ordering::less : cmp > 0 ? std::strong_ordering::greater : std::strong_ordering::equal;
    }
    std::string str() const { auto s=n_.convert_to<std::string>(); return d_==1 ? s : s+"/"+d_.convert_to<std::string>(); }
};
}
