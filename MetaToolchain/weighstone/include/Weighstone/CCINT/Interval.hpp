#pragma once
#include "BigInt.hpp"
#include <algorithm>
#include <optional>
#include <stdexcept>
namespace weighstone::ccint {
class Interval {
    std::optional<BigInt> lo_, hi_;
    bool bottom_ = false;
    Interval(std::optional<BigInt> lo,std::optional<BigInt> hi,bool bottom)
        : lo_(std::move(lo)),hi_(std::move(hi)),bottom_(bottom) {}
public:
    Interval() = default; // top: unbounded in both directions
    Interval(BigInt lo,BigInt hi) : lo_(std::move(lo)),hi_(std::move(hi)) {
        if (*lo_ > *hi_) throw std::invalid_argument("interval bounds reversed");
    }
    static Interval bottom() { return {std::nullopt,std::nullopt,true}; }
    static Interval point(BigInt x) { return {x,x}; }
    static Interval lower_bounded(BigInt x) { return {std::move(x),std::nullopt,false}; }
    static Interval upper_bounded(BigInt x) { return {std::nullopt,std::move(x),false}; }
    bool empty() const noexcept { return bottom_; }
    const std::optional<BigInt>& lower() const noexcept { return lo_; }
    const std::optional<BigInt>& upper() const noexcept { return hi_; }
    bool contains(const BigInt& x) const { return !bottom_ && (!lo_||x>=*lo_)&&(!hi_||x<=*hi_); }
    Interval meet(const Interval& b) const {
        if (bottom_||b.bottom_) return bottom();
        auto lo = lo_; auto hi = hi_;
        if (b.lo_ && (!lo || *b.lo_ > *lo)) lo=b.lo_;
        if (b.hi_ && (!hi || *b.hi_ < *hi)) hi=b.hi_;
        if (lo && hi && *lo > *hi) return bottom();
        return {lo,hi,false};
    }
    Interval join(const Interval& b) const {
        if (bottom_) return b;
        if (b.bottom_) return *this;
        auto lo = lo_ && b.lo_ ? std::optional<BigInt>(std::min(*lo_,*b.lo_)) : std::nullopt;
        auto hi = hi_ && b.hi_ ? std::optional<BigInt>(std::max(*hi_,*b.hi_)) : std::nullopt;
        return {lo,hi,false};
    }
    Interval add(const Interval& b) const {
        if(bottom_||b.bottom_) return bottom();
        return {lo_&&b.lo_ ? std::optional<BigInt>(*lo_+*b.lo_) : std::nullopt,
                hi_&&b.hi_ ? std::optional<BigInt>(*hi_+*b.hi_) : std::nullopt,false};
    }
    Interval negate() const {
        if(bottom_) return bottom();
        return {hi_ ? std::optional<BigInt>(-*hi_) : std::nullopt,
                lo_ ? std::optional<BigInt>(-*lo_) : std::nullopt,false};
    }
    Interval sub(const Interval& b) const { return add(b.negate()); }
    // Exact scaling, including one-sided infinities.
    Interval scale(const BigInt& k) const {
        if (bottom_) return bottom();
        if (k==0) return point(0);
        if (k<0) return negate().scale(BigInt(-k));
        return {lo_ ? std::optional<BigInt>(*lo_*k) : std::nullopt,
                hi_ ? std::optional<BigInt>(*hi_*k) : std::nullopt,false};
    }
    // Standard interval widening: *this is the previous iteration state.
    Interval widen(const Interval& next) const {
        if (bottom_) return next;
        if (next.bottom_) return *this;
        auto lo=lo_, hi=hi_;
        if (!next.lo_ || (lo && *next.lo_<*lo)) lo.reset();
        if (!next.hi_ || (hi && *next.hi_>*hi)) hi.reset();
        return {lo,hi,false};
    }
    friend bool operator==(const Interval& a,const Interval& b) {
        return a.bottom_==b.bottom_ && (a.bottom_ || (a.lo_==b.lo_ && a.hi_==b.hi_));
    }
    Interval mul(const Interval& b) const {
        if(bottom_||b.bottom_) return bottom();
        if (lo_&&hi_&&*lo_==*hi_) return b.scale(*lo_);
        if (b.lo_&&b.hi_&&*b.lo_==*b.hi_) return scale(*b.lo_);
        if (!lo_||!hi_||!b.lo_||!b.hi_) {
            if (lo_&&hi_&&*lo_==0&&*hi_==0) return point(0);
            if (b.lo_&&b.hi_&&*b.lo_==0&&*b.hi_==0) return point(0);
            return {}; // safe over-approximation for unbounded factors
        }
        BigInt a=*lo_**b.lo_, c=*lo_**b.hi_, d=*hi_**b.lo_, e=*hi_**b.hi_;
        return {std::min(std::min(a,c),std::min(d,e)),std::max(std::max(a,c),std::max(d,e))};
    }
};
}
