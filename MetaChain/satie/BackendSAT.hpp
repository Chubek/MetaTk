#pragma once
// Narrow bridge: do not include Satie's bundled DSLtk beside MetaChain's DSLtk.
// Only BackendSAT.cpp includes the solver (with a private DSLtk namespace);
// all other TUs share these plain types. A conflict-budget exhaustion throws
// length_error, never an UNSAT result.
#include <cstdint>
#include <algorithm>
#include <stdexcept>
#include <vector>

namespace metachain::sat {
struct Formula {
    int variables=0;
    std::vector<std::vector<std::int32_t>> clauses;
    int fresh() {
        if (variables>=200000) throw std::length_error("SAT variable limit exceeded");
        return ++variables;
    }
    void at_most(const std::vector<int>& literals,std::size_t k) {
        if (k>=literals.size()) return;
        if (!k) { for (auto x:literals) clauses.push_back({-x}); return; }
        std::vector<int> previous;
        for (auto x:literals) {
            std::vector<int> current(std::min(k,previous.size()+1));
            for (auto& v:current) v=fresh();
            clauses.push_back({-x,current[0]});
            for (std::size_t j=0;j<previous.size();++j) {
                clauses.push_back({-previous[j],current[j]});
                if (j+1<k) clauses.push_back({-x,-previous[j],current[j+1]});
                else clauses.push_back({-x,-previous[j]});
            }
            previous=std::move(current);
        }
    }
    void exactly_one(const std::vector<int>& literals) { clauses.push_back(literals); at_most(literals,1); }
};
struct Model {
    bool satisfiable=false;
    std::vector<bool> values; // one-based
};
Model solve(const Formula&);
} // namespace metachain::sat
