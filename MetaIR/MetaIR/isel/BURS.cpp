#include "metachain/ISel.hpp"
#include <map>

namespace metachain::isel {
// Bottom-up rewrite-system covering for trees. States are per-node minimal
// costs computed bottom-up, then a single top-down reduction emits tiles.
// Commutative integer ops may match with swapped operands.
namespace {
bool commutative_op(std::string_view op) {
    return op=="add"||op=="mul"||op=="and"||op=="or"||op=="xor"||op=="eq";
}
bool match_maybe_swapped(const Graph& g,Id node,const Pattern& p,Tile& tile) {
    if (p.opcode=="*"||p.inputs.size()!=2) return match(g,node,p,tile);
    if (match(g,node,p,tile)) return true;
    const auto& n=g.nodes[node];
    if (n.inputs.size()!=2||!commutative_op(n.opcode)) return false;
    if (p.opcode!=n.opcode) return false;
    Pattern swapped=p;
    std::swap(swapped.inputs[0],swapped.inputs[1]);
    Tile probe{node,0,{},{}};
    if (!match(g,node,swapped,probe)) return false;
    tile=std::move(probe);
    return true;
}
} // namespace
Result burs(const Graph& g,const std::vector<Rule>& rules,Options o) {
    Result r;
    r.diagnostics=backend::verify(g);
    if (!r.diagnostics.empty()) return r;
    if (!backend::topological_order(g)) { r.diagnostics.push_back({"selection requires an acyclic graph",0}); return r; }
    std::vector<unsigned> users(g.nodes.size());
    for (const auto& n:g.nodes) for (auto i:n.inputs) if (++users[i]>1) {
        r.diagnostics.push_back({"BURS requires tree operands without sharing",i}); return r;
    }
    // Generate candidates locally so commutative variants are covered.
    std::vector<std::vector<Id>> user_list(g.nodes.size());
    for (Id n=0;n<g.nodes.size();++n) for (auto i:g.nodes[n].inputs) user_list[i].push_back(n);
    std::vector<Tile> tiles;
    std::vector<std::vector<Id>> at(g.nodes.size());
    for (Id n=0;n<g.nodes.size();++n) for (Id rr=0;rr<rules.size();++rr) {
        if (rules[rr].name.empty()||rules[rr].pattern.opcode=="*") continue;
        Tile t{n,rr,{},{}};
        if (!match_maybe_swapped(g,n,rules[rr].pattern,t)) continue;
        bool valid=true;
        auto contains=[&](Id x){return std::find(t.covered.begin(),t.covered.end(),x)!=t.covered.end();};
        for (auto x:t.covered) if (x!=n) {
            if (g.nodes[x].effect || std::find(g.outputs.begin(),g.outputs.end(),x)!=g.outputs.end()) valid=false;
            for (auto u:user_list[x]) if (!contains(u)) valid=false;
        }
        for (auto x:t.operands) if (contains(x)) valid=false;
        if (!valid) continue;
        if (tiles.size()>=o.max_tiles) { r.diagnostics.push_back({"selection candidate limit exceeded",n}); return r; }
        at[n].push_back(tiles.size());
        tiles.push_back(std::move(t));
    }
    const auto inf=std::numeric_limits<std::uint64_t>::max();
    std::vector<std::uint64_t> cost(g.nodes.size(),inf);
    std::vector<std::optional<Id>> choice(g.nodes.size());
    auto topo=*backend::topological_order(g);
    std::size_t steps=0;
    for (auto n:topo) for (auto t:at[n]) {
        if (++steps>o.max_search) { r.diagnostics.push_back({"BURS labeling work limit exceeded",n}); return r; }
        std::uint64_t c=rules[tiles[t].rule].cost;
        for (auto operand:tiles[t].operands) {
            if (cost[operand]==inf||c>inf-cost[operand]) { c=inf; break; }
            c+=cost[operand];
        }
        if (c<cost[n]) { cost[n]=c; choice[n]=t; }
    }
    std::vector<Id> work;
    for (Id n=0;n<g.nodes.size();++n) if (!users[n]) work.push_back(n);
    while (!work.empty()) {
        auto n=work.back(); work.pop_back();
        if (!choice[n]) { r.diagnostics.push_back({"no BURS cover for tree root",n}); return r; }
        const auto& t=tiles[*choice[n]];
        if (r.cost>inf-rules[t.rule].cost) { r.diagnostics.push_back({"BURS cost overflow",n}); return r; }
        r.tiles.push_back(t); r.cost+=rules[t.rule].cost;
        work.insert(work.end(),t.operands.begin(),t.operands.end());
    }
    r.optimal=true; order_tiles(r,g); return r;
}
} // namespace metachain::isel
