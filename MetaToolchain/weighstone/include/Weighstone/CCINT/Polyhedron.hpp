#pragma once
#include "Constraints.hpp"
#include <algorithm>
#include <cstddef>
#include <set>
#include <utility>

namespace weighstone::ccint {
using IntegerPoint = std::map<std::string,BigInt>;

// Limits bound candidate points and inequalities, not coefficient bit sizes.
struct AnalysisBudget {
    std::size_t max_points=100000;
    std::size_t max_constraints=4096;
};

struct IntegerPoints {
    // proven means points is the complete set; unknown means no complete set.
    Verdict complete;
    std::vector<IntegerPoint> points;
};

// A conjunction of affine constraints over mathematical INTEGER coordinates.
// Not a complete Presburger solver: elimination proves emptiness, never integer
// feasibility. Feasibility requires a concrete witness or complete enumeration.
class Polyhedron {
    std::vector<std::string> variables_;
    std::vector<LinearConstraint> constraints_;
    struct Inequality { std::vector<BigInt> a; BigInt b; }; // a*x <= b

    static void normalize(Inequality& row) {
        BigInt g=0;
        for(const auto& a:row.a) g=gcd(g,a);
        if(g>1) {
            for(auto& a:row.a) a/=g;
            row.b=floor_div(row.b,g); // valid strengthening for integer points
        }
    }
    std::vector<Inequality> inequalities() const {
        std::vector<Inequality> rows;
        for(const auto& c:constraints_) {
            Inequality row{{},BigInt(c.rhs-c.lhs.constant())};
            for(const auto& v:variables_) {
                auto it=c.lhs.terms().find(v);
                row.a.push_back(it==c.lhs.terms().end()?BigInt(0):it->second);
            }
            if(c.relation!=Relation::greater_equal) {
                auto r=row; normalize(r); rows.push_back(std::move(r));
            }
            if(c.relation!=Relation::less_equal) {
                for(auto& a:row.a) a=-a;
                row.b=-row.b; normalize(row); rows.push_back(std::move(row));
            }
        }
        return rows;
    }
    Verdict elimination_empty(const AnalysisBudget& budget) const {
        auto rows=inequalities();
        if(rows.size()>budget.max_constraints)
            return {Decision::unknown,"constraint budget exhausted"};
        for(std::size_t col=0;;++col) {
            for(const auto& r:rows) {
                if(r.b<0 && std::all_of(r.a.begin(),r.a.end(),[](const BigInt& x){return x==0;}))
                    return {Decision::proven,"integer inequality elimination contradiction"};
            }
            if(col==variables_.size()) break;
            std::vector<const Inequality*> positive,negative;
            std::vector<Inequality> next;
            for(const auto& r:rows) {
                if(r.a[col]>0) positive.push_back(&r);
                else if(r.a[col]<0) negative.push_back(&r);
                else next.push_back(r);
            }
            if(!negative.empty() && positive.size()>(budget.max_constraints-next.size())/negative.size())
                return {Decision::unknown,"elimination budget exhausted"};
            for(const auto* p:positive) for(const auto* n:negative) {
                Inequality r{std::vector<BigInt>(variables_.size()),0};
                const BigInt pc=p->a[col], nc=-n->a[col];
                for(std::size_t j=0;j<r.a.size();++j) r.a[j]=nc*p->a[j]+pc*n->a[j];
                r.b=nc*p->b+pc*n->b;
                normalize(r); next.push_back(std::move(r));
            }
            rows=std::move(next);
        }
        return {Decision::unknown,"elimination did not establish integer emptiness"};
    }
public:
    explicit Polyhedron(std::vector<std::string> variables) : variables_(std::move(variables)) {
        std::set<std::string> seen;
        for(const auto& v:variables_)
            if(v.empty() || !seen.insert(v).second) throw std::invalid_argument("invalid or duplicate polyhedron variable");
    }
    const auto& variables() const noexcept { return variables_; }
    const auto& constraints() const noexcept { return constraints_; }
    void validate(const LinearExpr& expr) const {
        for(const auto& term:expr.terms())
            if(std::find(variables_.begin(),variables_.end(),term.first)==variables_.end())
                throw std::invalid_argument("undeclared polyhedron variable: "+term.first);
    }
    Polyhedron& add(LinearConstraint c) {
        validate(c.lhs);
        constraints_.push_back(std::move(c)); return *this;
    }
    Polyhedron intersect(const Polyhedron& other) const {
        if(variables_!=other.variables_) throw std::invalid_argument("polyhedron coordinate mismatch");
        auto result=*this;
        result.constraints_.insert(result.constraints_.end(),other.constraints_.begin(),other.constraints_.end());
        return result;
    }
    bool contains(const IntegerPoint& point) const {
        for(const auto& v:variables_)
            if(!point.contains(v)) throw std::invalid_argument("missing polyhedron coordinate: "+v);
        for(const auto& c:constraints_) {
            const BigInt value=*c.lhs.eval(point);
            if(c.relation==Relation::less_equal && value>c.rhs) return false;
            if(c.relation==Relation::greater_equal && value<c.rhs) return false;
            if(c.relation==Relation::equal && value!=c.rhs) return false;
        }
        return true;
    }
    // Conservative box from unary constraints. Relational bounds may be missed.
    VariableRanges bounding_box() const {
        VariableRanges box;
        for(const auto& v:variables_) box.emplace(v,Interval{});
        for(const auto& row:inequalities()) {
            std::size_t count=0, index=0;
            for(std::size_t j=0;j<row.a.size();++j) if(row.a[j]!=0) {++count; index=j;}
            if(count!=1) continue;
            const BigInt& a=row.a[index];
            const auto bound=a>0 ? Interval::upper_bounded(floor_div(row.b,a))
                                 : Interval::lower_bounded(ceil_div(row.b,a));
            auto& range=box.at(variables_[index]); range=range.meet(bound);
        }
        return box;
    }
    IntegerPoints integer_points(AnalysisBudget budget={}) const {
        const auto box=bounding_box();
        for(const auto& entry:box) if(entry.second.empty())
            return {{Decision::proven,"empty bounding box"},{}};
        // Constant contradictions include the zero-dimensional empty set.
        for(const auto& c:constraints_) if(check(c,box).is_disproven())
            return {{Decision::proven,"constraint excludes bounding box"},{}};
        BigInt count=1;
        for(const auto& v:variables_) {
            const auto& r=box.at(v);
            if(!r.lower() || !r.upper()) return {{Decision::unknown,"unbounded enumeration box"},{}};
            count*=*r.upper()-*r.lower()+1;
        }
        if(count>budget.max_points) return {{Decision::unknown,"point budget exhausted"},{}};
        IntegerPoints result{{Decision::proven,"complete bounded enumeration"},{}};
        IntegerPoint point;
        for(const auto& v:variables_) point[v]=*box.at(v).lower();
        // An odometer avoids recursion even for high-dimensional unit boxes.
        const auto total=count.convert_to<std::size_t>();
        for(std::size_t i=0;i<total;++i) {
            if(contains(point)) result.points.push_back(point);
            for(std::size_t j=variables_.size();j>0;--j) {
                auto& value=point.at(variables_[j-1]);
                const auto& r=box.at(variables_[j-1]);
                if(value<*r.upper()) {++value; break;}
                value=*r.lower();
            }
        }
        return result;
    }
    // Verdict describes the proposition "this integer set is empty".
    Verdict is_empty(AnalysisBudget budget={}) const {
        IntegerPoint zero;
        for(const auto& v:variables_) zero[v]=0;
        if(contains(zero)) return {Decision::disproven,"zero is an integer witness"};
        const auto eliminated=elimination_empty(budget);
        if(eliminated.is_proven()) return eliminated;
        const auto points=integer_points(budget);
        if(points.complete.is_proven())
            return {points.points.empty()?Decision::proven:Decision::disproven,"complete integer enumeration"};
        return {Decision::unknown,"no integer witness or emptiness proof within budget"};
    }
    // As with check(), disproven means false at EVERY point, not a counterexample.
    Verdict prove(const LinearConstraint& c,AnalysisBudget budget={}) const {
        validate(c.lhs);
        const auto empty=is_empty(budget);
        if(empty.is_proven()) return {Decision::proven,"empty domain (vacuous truth)"};
        const auto ranged=check(c,bounding_box());
        if(ranged.decision!=Decision::unknown) return ranged;
        auto branch_empty=[&](Relation relation,BigInt rhs) {
            auto branch=*this; branch.add({c.lhs,relation,std::move(rhs)});
            return branch.is_empty(budget).is_proven();
        };
        bool holds=false;
        if(c.relation==Relation::less_equal) holds=branch_empty(Relation::greater_equal,BigInt(c.rhs+1));
        else if(c.relation==Relation::greater_equal) holds=branch_empty(Relation::less_equal,BigInt(c.rhs-1));
        else holds=branch_empty(Relation::less_equal,BigInt(c.rhs-1)) &&
                   branch_empty(Relation::greater_equal,BigInt(c.rhs+1));
        if(holds) return {Decision::proven,"negated constraint is empty"};
        if(branch_empty(c.relation,c.rhs)) return {Decision::disproven,"constraint is false throughout domain"};
        return {Decision::unknown,"constraint is mixed or analysis is inconclusive"};
    }
};
}
