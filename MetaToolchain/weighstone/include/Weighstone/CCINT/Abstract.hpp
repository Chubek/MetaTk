#pragma once
#include "Interval.hpp"
#include "Congruence.hpp"
namespace weighstone::ccint {
// Product domain: sound but not fully reduced to strongest possible product.
class AbstractInteger {
    Interval interval_;
    Congruence congruence_=Congruence::top();
    bool bottom_=false;
    void reduce() {
        if(interval_.empty()) {bottom_=true;return;}
        auto l=interval_.lower(), u=interval_.upper();
        if(congruence_.modulus()==0) {
            if(!interval_.contains(congruence_.residue())) bottom_=true;
            else interval_=Interval::point(congruence_.residue());
        } else {
            if(l) interval_=interval_.meet(Interval::lower_bounded(
                *l+euclidean_mod(congruence_.residue()-*l,congruence_.modulus())));
            if(u) interval_=interval_.meet(Interval::upper_bounded(
                *u-euclidean_mod(*u-congruence_.residue(),congruence_.modulus())));
            bottom_=interval_.empty();
        }
        if(bottom_) interval_=Interval::bottom();
    }
public:
    AbstractInteger(Interval i={}, Congruence c=Congruence::top())
        : interval_(std::move(i)),congruence_(std::move(c)) {reduce();}
    bool empty() const { return bottom_; }
    const Interval& interval() const { return interval_; }
    const Congruence& congruence() const { return congruence_; }
    AbstractInteger add(const AbstractInteger& b) const {
        if(bottom_||b.bottom_) return {Interval::bottom()};
        return {interval_.add(b.interval_), congruence_.add(b.congruence_)};
    }
    AbstractInteger join(const AbstractInteger& b) const {
        if(bottom_) return b;
        if(b.bottom_) return *this;
        return {interval_.join(b.interval_),congruence_.join(b.congruence_)};
    }
    AbstractInteger meet(const AbstractInteger& b) const {
        if(bottom_||b.bottom_) return {Interval::bottom()};
        const auto c=congruence_.meet(b.congruence_);
        return c ? AbstractInteger(interval_.meet(b.interval_),*c) : AbstractInteger(Interval::bottom());
    }
};
}
