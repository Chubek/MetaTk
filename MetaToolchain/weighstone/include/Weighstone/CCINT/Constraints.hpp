#pragma once
#include "Linear.hpp"
#include "../../../support/Result.hpp"
#include <vector>
namespace weighstone::ccint {
using weighstone::support::Decision;
using weighstone::support::Verdict;
enum class Relation { less_equal, equal, greater_equal };
struct LinearConstraint {LinearExpr lhs; Relation relation; BigInt rhs;};
inline Verdict check(const LinearConstraint& c,const VariableRanges& ranges) {
    const auto x=c.lhs.range(ranges);
    if(x.empty()) return {Decision::proven,"empty input domain (vacuous truth)"};
    const auto& lo=x.lower(); const auto& hi=x.upper();
    switch(c.relation) {
    case Relation::less_equal:
        if(hi && *hi<=c.rhs) return {Decision::proven,"upper bound"};
        if(lo && *lo>c.rhs) return {Decision::disproven,"lower bound"};
        break;
    case Relation::greater_equal:
        if(lo && *lo>=c.rhs) return {Decision::proven,"lower bound"};
        if(hi && *hi<c.rhs) return {Decision::disproven,"upper bound"};
        break;
    case Relation::equal:
        if(lo && hi && *lo==c.rhs && *hi==c.rhs) return {Decision::proven,"singleton range"};
        if((lo && *lo>c.rhs)||(hi && *hi<c.rhs)) return {Decision::disproven,"outside range"};
        break;
    }
    return {Decision::unknown,"interval bounds insufficient"};
}
}
