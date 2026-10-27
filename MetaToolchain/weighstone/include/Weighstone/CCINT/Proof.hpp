#pragma once
#include "Symbolic.hpp"
#include "../../../support/Result.hpp"
namespace weighstone::ccint {
inline weighstone::support::Verdict prove_equal(const Expr& a,const Expr& b) {
    auto d=(a-b).simplify();
    if(d.is_constant() && d.constant()==0)
        return {weighstone::support::Decision::proven,"symbolic normalization"};
    return {weighstone::support::Decision::unknown,"normalizer cannot establish equivalence"};
}
}
