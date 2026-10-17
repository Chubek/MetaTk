#pragma once
#include "Backend.hpp"
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace metachain::isel {
using backend::Id;
using backend::Graph;
// '*' is an operand boundary. Other patterns consume the matched node.
struct Pattern { std::string opcode; std::vector<Pattern> inputs; };
struct Rule { std::string name; Pattern pattern; unsigned cost=1; };
struct Tile { Id root=0, rule=0; std::vector<Id> covered, operands; };
struct Result {
    std::vector<Tile> tiles; // dependencies precede users
    std::uint64_t cost=0;
    bool optimal=false;
    std::vector<backend::Diagnostic> diagnostics;
    explicit operator bool() const { return diagnostics.empty(); }
};
struct Options { std::size_t max_search=100000, max_tiles=100000; };
// --- Rule/pattern analysis (used by all covering strategies) ---
inline std::size_t pattern_size(const Pattern& p) {
    std::size_t n=1;
    for (const auto& c:p.inputs) n+=pattern_size(c);
    return n;
}
inline std::size_t pattern_depth(const Pattern& p) {
    std::size_t d=0;
    for (const auto& c:p.inputs) d=std::max(d,pattern_depth(c));
    return 1+d;
}
inline std::size_t pattern_operands(const Pattern& p) {
    if (p.opcode=="*") return 1;
    std::size_t n=0;
    for (const auto& c:p.inputs) n+=pattern_operands(c);
    return n;
}
inline std::string dump_pattern(const Pattern& p) {
    if (p.opcode=="*") return "*";
    std::string s=p.opcode;
    if (!p.inputs.empty()) {
        s+="(";
        for (std::size_t k=0;k<p.inputs.size();++k) s+=(k?",":"")+dump_pattern(p.inputs[k]);
        s+=")";
    }
    return s;
}
inline std::string dump_rule(const Rule& r) {
    std::ostringstream out;
    out << (r.name.empty()?"<anon>":r.name) << " := " << dump_pattern(r.pattern) << "  [cost " << r.cost << "]";
    return out.str();
}
inline std::string dump_tile(const Graph& g,const std::vector<Rule>& rules,const Tile& t) {
    std::ostringstream out;
    out << (t.rule<rules.size()?rules[t.rule].name:"<bad-rule>") << " @%" << t.root << " covers{";
    for (std::size_t k=0;k<t.covered.size();++k) out << (k?",":"") << "%" << t.covered[k];
    out << "} operands{";
    for (std::size_t k=0;k<t.operands.size();++k) out << (k?",":"") << "%" << t.operands[k];
    out << "}";
    (void)g;
    return out.str();
}
inline std::string dump_result(const Graph& g,const std::vector<Rule>& rules,const Result& r) {
    std::ostringstream out;
    out << "cover cost=" << r.cost << (r.optimal?" optimal":" heuristic") << " tiles=" << r.tiles.size() << "\n";
    for (const auto& t:r.tiles) out << "  " << dump_tile(g,rules,t) << "\n";
    for (const auto& d:r.diagnostics) out << "  diagnostic: " << d.message << " @%" << d.node << "\n";
    return out.str();
}
// Index rules by top opcode for faster candidate generation and reporting.
inline std::map<std::string,std::vector<Id>> index_rules(const std::vector<Rule>& rules) {
    std::map<std::string,std::vector<Id>> index;
    for (Id r=0;r<rules.size();++r) index[rules[r].pattern.opcode].push_back(r);
    return index;
}
// Exact-cover checker shared by tests and the SAT encoder.
inline bool verify_cover(const Graph& g,const Result& r) {
    if (!r) return false;
    if (!backend::topological_order(g)) return false;
    std::vector<bool> covered(g.nodes.size());
    for (const auto& t:r.tiles) for (auto n:t.covered) {
        if (n>=g.nodes.size()||covered[n]) return false;
        covered[n]=true;
    }
    for (auto c:covered) if (!c) return false;
    return true;
}
inline bool match(const Graph& g,Id node,const Pattern& p,Tile& tile,std::size_t depth=0) {
    if (depth>256) return false;
    if (p.opcode=="*") { tile.operands.push_back(node); return true; }
    const auto& n=g.nodes[node];
    if (p.opcode!=n.opcode || p.inputs.size()!=n.inputs.size()) return false;
    // A DAG pattern may reach the same internal value more than once. Charge
    // it once, while still checking every occurrence against its subpattern.
    if (std::find(tile.covered.begin(),tile.covered.end(),node)==tile.covered.end()) tile.covered.push_back(node);
    for (Id k=0;k<p.inputs.size();++k) if (!match(g,n.inputs[k],p.inputs[k],tile,depth+1)) return false;
    return true;
}
inline std::vector<Tile> candidates(const Graph& g,const std::vector<Rule>& rules,Options o,Result& out) {
    std::vector<Tile> tiles;
    out.diagnostics=backend::verify(g);
    if (!out.diagnostics.empty()) return tiles;
    for (Id r=0;r<rules.size();++r) {
        bool valid=!rules[r].name.empty()&&rules[r].pattern.opcode!="*";
        std::vector<std::pair<const Pattern*,std::size_t>> work{{&rules[r].pattern,0}};
        while (!work.empty()) {
            auto [p,depth]=work.back(); work.pop_back();
            if (depth>256||p->opcode.empty()||(p->opcode=="*"&&!p->inputs.empty())) { valid=false; break; }
            for (const auto& child:p->inputs) work.emplace_back(&child,depth+1);
        }
        if (!valid) out.diagnostics.push_back({"invalid selection rule",r});
    }
    if (!out.diagnostics.empty()) return tiles;
    if (!backend::topological_order(g)) { out.diagnostics.push_back({"selection requires an acyclic graph",0}); return tiles; }
    std::vector<std::vector<Id>> users(g.nodes.size());
    for (Id n=0;n<g.nodes.size();++n) for (auto i:g.nodes[n].inputs) users[i].push_back(n);
    for (Id n=0;n<g.nodes.size();++n) for (Id r=0;r<rules.size();++r) {
        Tile t{n,r,{},{}};
        if (!match(g,n,rules[r].pattern,t)) continue;
        bool valid=true;
        auto contains=[&](Id x){return std::find(t.covered.begin(),t.covered.end(),x)!=t.covered.end();};
        for (auto x:t.covered) if (x!=n) {
            if (g.nodes[x].effect || std::find(g.outputs.begin(),g.outputs.end(),x)!=g.outputs.end()) valid=false;
            for (auto u:users[x]) if (!contains(u)) valid=false;
        }
        for (auto x:t.operands) if (contains(x)) valid=false;
        if (valid) {
            if (tiles.size()>=o.max_tiles) { out.diagnostics.push_back({"selection candidate limit exceeded",n}); return {}; }
            tiles.push_back(std::move(t));
        }
    }
    return tiles;
}
inline void order_tiles(Result& r,const Graph& g) {
    const Id missing=g.nodes.size();
    std::vector<Id> owner(g.nodes.size(),missing);
    for (Id t=0;t<r.tiles.size();++t) for (auto n:r.tiles[t].covered) {
        if (n>=g.nodes.size()||owner[n]!=missing) { r.diagnostics.push_back({"invalid tile cover",n}); return; }
        owner[n]=t;
    }
    Graph dependencies; dependencies.nodes.resize(r.tiles.size());
    for (Id t=0;t<r.tiles.size();++t) {
        dependencies.nodes[t].opcode="tile";
        for (auto n:r.tiles[t].operands) {
            if (n>=g.nodes.size()||owner[n]==missing||owner[n]==t) { r.diagnostics.push_back({"invalid tile operand",n}); return; }
            dependencies.nodes[t].inputs.push_back(owner[n]);
        }
    }
    auto order=backend::topological_order(dependencies);
    if (!order) { r.diagnostics.push_back({"cyclic tile dependencies",0}); return; }
    auto old=std::move(r.tiles); for (auto t:*order) r.tiles.push_back(std::move(old[t]));
}
inline Result cover(const Graph& g,const std::vector<Rule>& rules,Options options={},bool greedy=false) {
    Result r; auto tiles=candidates(g,rules,options,r); if (!r) return r;
    std::vector<std::vector<Id>> at(g.nodes.size());
    for (Id t=0;t<tiles.size();++t) for (auto n:tiles[t].covered) at[n].push_back(t);
    std::vector<bool> covered(g.nodes.size()); std::vector<Id> path,best;
    auto best_cost=std::numeric_limits<std::uint64_t>::max(); std::size_t searched=0; bool limited=false,found=false;
    struct Frame {
        std::uint64_t cost=0;
        std::optional<Id> selected;
        std::vector<Id> choices;
        std::size_t next=0;
        bool initialized=false;
    };
    std::vector<Frame> stack(1);
    auto pop=[&] {
        if (stack.back().selected) {
            for (auto n:tiles[*stack.back().selected].covered) covered[n]=false;
            path.pop_back();
        }
        stack.pop_back();
    };
    // Search depth can equal the graph size. Keep frames on the heap rather
    // than exhausting the C++ call stack before the work limit is reached.
    while (!stack.empty()) {
        auto& frame=stack.back();
        if (!frame.initialized) {
            if (++searched>options.max_search) { limited=true; break; }
            if (frame.cost>=best_cost) { pop(); continue; }
            Id next=g.nodes.size();
            for (Id n=0;n<g.nodes.size();++n) if (!covered[n]) {
                std::vector<Id> available;
                for (auto t:at[n]) if (std::none_of(tiles[t].covered.begin(),tiles[t].covered.end(),[&](Id x){return covered[x];})) available.push_back(t);
                if (next==g.nodes.size() || available.size()<frame.choices.size()) { next=n; frame.choices=std::move(available); }
                if (frame.choices.empty()) break;
            }
            if (next==g.nodes.size()) {
                best=path; best_cost=frame.cost; found=true;
                if (greedy) break;
                pop(); continue;
            }
            std::stable_sort(frame.choices.begin(),frame.choices.end(),[&](Id a,Id b){
                return std::uint64_t(rules[tiles[a].rule].cost)*tiles[b].covered.size()<std::uint64_t(rules[tiles[b].rule].cost)*tiles[a].covered.size();
            });
            frame.initialized=true;
        }
        if (frame.next==frame.choices.size()) { pop(); continue; }
        const auto t=frame.choices[frame.next++];
        const auto cost=frame.cost+rules[tiles[t].rule].cost;
        if (cost<frame.cost) continue;
        for (auto n:tiles[t].covered) covered[n]=true;
        path.push_back(t); stack.push_back({cost,t,{},0,false});
    }
    if (!found) r.diagnostics.push_back({limited?"selection search limit exceeded":"no complete instruction cover",0});
    else { for (auto t:best) r.tiles.push_back(tiles[t]); r.cost=best_cost; r.optimal=!limited&&!greedy; order_tiles(r,g); }
    return r;
}
Result tree_cover(const Graph&,const std::vector<Rule>&,Options={});
Result burs(const Graph&,const std::vector<Rule>&,Options={});
Result dag_cover(const Graph&,const std::vector<Rule>&,Options={});
Result graph_cover(const Graph&,const std::vector<Rule>&,Options={});
Result macro_cover(const Graph&,const std::vector<Rule>&,Options={});
Result smt_cover(const Graph&,const std::vector<Rule>&,Options={});
} // namespace metachain::isel
