#pragma once
#include "BigInt.hpp"
#include <optional>
namespace weighstone::ccint {
// x(n) = initial + step*n, valid for n>=0 over mathematical integers.
struct AffineRecurrence {
    BigInt initial, step;
    BigInt at(const BigInt& iteration) const {
        if(iteration<0) throw std::invalid_argument("negative iteration");
        return initial+step*iteration;
    }
    // Number of iterations for x(n) < bound, n>=0. A false initial guard
    // gives zero; a true guard with a nonpositive step gives nullopt.
    std::optional<BigInt> trip_count_less_than(const BigInt& bound) const {
        if(initial>=bound) return BigInt(0);
        if(step<=0) return std::nullopt;
        return ceil_div(bound-initial,step);
    }
    std::optional<BigInt> trip_count_less_equal(const BigInt& bound) const {
        return trip_count_less_than(BigInt(bound+1));
    }
    std::optional<BigInt> trip_count_greater_than(const BigInt& bound) const {
        return AffineRecurrence{-initial,-step}.trip_count_less_than(BigInt(-bound));
    }
    std::optional<BigInt> trip_count_greater_equal(const BigInt& bound) const {
        return trip_count_greater_than(BigInt(bound-1));
    }
};
}
