#include "metachain/ISel.hpp"
#include <map>

namespace metachain::isel {
// Macro-expansion covering: order rules like macro definitions (largest
// patterns first, cheapest per covered node wins ties), drop rules that can
// never match, then run the greedy exact search. Heuristic: not optimal.
Result macro_cover(const Graph& g,const std::vector<Rule>& rules,Options o) {
    // Filter out malformed rules early with the same validation as candidates().
    for (const auto& rule:rules) {
        if (rule.name.empty()||rule.pattern.opcode=="*"||rule.pattern.opcode.empty()) {
            Result r; r.diagnostics.push_back({"invalid macro rule",0}); return r;
        }
    }
    // Rank rules macro-style: more covered nodes first, then lower cost.
    std::vector<Id> order(rules.size());
    std::iota(order.begin(),order.end(),Id{0});
    std::stable_sort(order.begin(),order.end(),[&](Id a,Id b){
        const auto sa=pattern_size(rules[a].pattern), sb=pattern_size(rules[b].pattern);
        if (sa!=sb) return sa>sb;
        return rules[a].cost<rules[b].cost;
    });
    std::vector<Rule> ranked;
    ranked.reserve(rules.size());
    for (auto i:order) ranked.push_back(rules[i]);
    auto r=cover(g,ranked,o,true);
    // Rule indices above refer to the ranked order; map them back so tile
    // dumps and cost attribution use the caller's rule vector.
    for (auto& t:r.tiles) if (t.rule<order.size()) t.rule=order[t.rule];
    return r;
}
} // namespace metachain::isel
