#pragma once
#include "Interval.hpp"
#include <map>
#include <string>
namespace weighstone::ccint {
using VariableRanges = std::map<std::string, Interval>;
class LinearExpr {
    std::map<std::string,BigInt> terms_;
    BigInt constant_=0;
public:
    LinearExpr()=default;
    explicit LinearExpr(BigInt c):constant_(std::move(c)){}
    static LinearExpr variable(std::string name,BigInt coefficient=1) {
        if(name.empty()) throw std::invalid_argument("empty variable name");
        LinearExpr x; if(coefficient!=0) x.terms_[std::move(name)]=std::move(coefficient); return x;
    }
    const auto& terms() const { return terms_; }
    const BigInt& constant() const {return constant_;}
    LinearExpr& operator+=(const LinearExpr& b) {
        constant_+=b.constant_;
        for(const auto& [name,v]:b.terms_) {
            auto& target=terms_[name]; target+=v;
            if(target==0) terms_.erase(name);
        }
        return *this;
    }
    friend LinearExpr operator+(LinearExpr a,const LinearExpr& b) {return a+=b;}
    friend LinearExpr operator-(LinearExpr a,const LinearExpr& b) {return a+=(-b);}
    LinearExpr operator-() const {
        LinearExpr x; x.constant_=-constant_;
        for(const auto& [key,value]:terms_) x.terms_[key]=-value;
        return x;
    }
    LinearExpr scale(const BigInt& k) const {
        LinearExpr x; x.constant_=constant_*k;
        if(k!=0) for(const auto& [name,v]:terms_) x.terms_[name]=v*k;
        return x;
    }
    std::optional<BigInt> eval(const std::map<std::string,BigInt>& env) const {
        BigInt value=constant_;
        for(const auto& [name,coefficient]:terms_) {
            auto it=env.find(name); if(it==env.end()) return std::nullopt;
            value += coefficient*it->second;
        }
        return value;
    }
    Interval range(const VariableRanges& vars) const {
        for(const auto& entry:vars) if(entry.second.empty()) return Interval::bottom();
        Interval x=Interval::point(constant_);
        for(const auto& [name,coeff]:terms_) {
            const auto it=vars.find(name);
            const Interval v=it==vars.end()?Interval{}:it->second;
            x=x.add(v.scale(coeff));
        }
        return x;
    }
};
}
