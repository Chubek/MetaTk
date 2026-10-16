#pragma once

// Target-independent backend data. IDs are dense, zero-based indices.
#include "MetaIL.hpp"
#include <algorithm>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <numeric>
#include <optional>
#include <queue>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace metachain::backend {
using Id = std::size_t;
struct Diagnostic { std::string message; Id node = 0; };
struct Node {
    std::string opcode;
    std::vector<Id> inputs;
    std::int64_t immediate = 0;
    bool effect = false;
};
struct Graph {
    std::vector<Node> nodes;
    std::vector<Id> outputs;
};
inline std::vector<Diagnostic> verify(const Graph& g) {
    std::vector<Diagnostic> errors;
    for (Id n=0;n<g.nodes.size();++n) {
        if (g.nodes[n].opcode.empty()) errors.push_back({"empty node opcode",n});
        for (auto i:g.nodes[n].inputs)
            if (i>=g.nodes.size()) errors.push_back({"input outside graph",n});
    }
    for (auto n:g.outputs) if (n>=g.nodes.size()) errors.push_back({"output outside graph",n});
    return errors;
}
inline std::optional<std::vector<Id>> topological_order(const Graph& g) {
    if (!verify(g).empty()) return std::nullopt;
    std::vector<Id> degree(g.nodes.size());
    std::vector<std::vector<Id>> users(g.nodes.size());
    for (Id n=0;n<g.nodes.size();++n) for (auto i:g.nodes[n].inputs) {
        ++degree[n]; users[i].push_back(n);
    }
    std::priority_queue<Id,std::vector<Id>,std::greater<Id>> ready;
    for (Id n=0;n<degree.size();++n) if (!degree[n]) ready.push(n);
    std::vector<Id> order;
    while (!ready.empty()) {
        auto n=ready.top(); ready.pop(); order.push_back(n);
        for (auto u:users[n]) if (!--degree[u]) ready.push(u);
    }
    if (order.size()!=g.nodes.size()) return std::nullopt;
    return order;
}
struct Access {
    std::set<unsigned> uses, defs;
    bool barrier = false, may_trap = false;
};
inline Access access(const metail::Instruction& i) {
    using metail::Op;
    Access a;
    if (metail::binary(i.op)) { a.uses={i.b,i.c}; a.defs={i.a}; }
    else if (i.op==Op::Mov) { a.uses={i.b}; a.defs={i.a}; }
    else if (i.op==Op::Const || i.op==Op::Pop) a.defs={i.a};
    else if (i.op==Op::Jz || i.op==Op::Jnz || i.op==Op::Push || i.op==Op::Print || i.op==Op::Halt) a.uses={i.a};
    a.may_trap=i.op==Op::Div || i.op==Op::Mod || i.op==Op::Shl || i.op==Op::Shr;
    a.barrier=metail::branch(i.op) || i.op==Op::Ret || i.op==Op::Push || i.op==Op::Pop || i.op==Op::Print || i.op==Op::Halt;
    if (i.op==Op::Call || i.op==Op::Ret) for (unsigned r=0;r<32;++r) { a.uses.insert(r); a.defs.insert(r); }
    return a;
}
inline bool intersects(const std::set<unsigned>& a,const std::set<unsigned>& b) {
    for (auto x:a) if (b.count(x)) return true;
    return false;
}
struct Stats {
    std::size_t nodes=0, edges=0, outputs=0, effects=0, max_fanout=0;
};
inline Stats stats(const Graph& g) {
    Stats s; s.nodes=g.nodes.size(); s.outputs=g.outputs.size();
    for (const auto& n:g.nodes) {
        s.edges+=n.inputs.size(); s.effects+=n.effect?1:0;
    }
    if (!g.nodes.empty()) {
        std::vector<std::size_t> users(g.nodes.size());
        for (const auto& n:g.nodes) for (auto i:n.inputs) if (i<g.nodes.size()) ++users[i];
        for (auto u:users) s.max_fanout=std::max(s.max_fanout,u);
    }
    return s;
}
inline std::string dump(const Graph& g) {
    std::ostringstream out;
    out << "graph nodes=" << g.nodes.size() << " outputs=[";
    for (std::size_t k=0;k<g.outputs.size();++k) out << (k?",":"") << g.outputs[k];
    out << "]\n";
    for (Id n=0;n<g.nodes.size();++n) {
        out << "  %" << n << " = " << (g.nodes[n].opcode.empty()?"<empty>":g.nodes[n].opcode);
        if (!g.nodes[n].inputs.empty()) {
            out << "(";
            for (std::size_t k=0;k<g.nodes[n].inputs.size();++k) out << (k?",":"") << "%" << g.nodes[n].inputs[k];
            out << ")";
        }
        if (g.nodes[n].immediate) out << " imm=" << g.nodes[n].immediate;
        if (g.nodes[n].effect) out << " [effect]";
        out << "\n";
    }
    return out.str();
}
inline std::string to_dot(const Graph& g,std::string_view name="g") {
    std::ostringstream out;
    out << "digraph " << (name.empty()?"g":name) << " {\n";
    for (Id n=0;n<g.nodes.size();++n) {
        out << "  n" << n << " [label=\"" << n << ":" << (g.nodes[n].opcode.empty()?"?":g.nodes[n].opcode);
        if (g.nodes[n].effect) out << "!";
        out << "\"];\n";
        for (auto i:g.nodes[n].inputs) if (i<g.nodes.size()) out << "  n" << i << " -> n" << n << ";\n";
    }
    for (auto o:g.outputs) if (o<g.nodes.size()) out << "  n" << o << " [peripheries=2];\n";
    out << "}\n";
    return out.str();
}
// Nodes that can reach a designated output (or every node when no outputs).
inline std::vector<Id> live_from_outputs(const Graph& g) {
    if (!verify(g).empty()) return {};
    std::vector<std::vector<Id>> users(g.nodes.size());
    for (Id n=0;n<g.nodes.size();++n) for (auto i:g.nodes[n].inputs) users[i].push_back(n);
    std::vector<Id> seeds=g.outputs.empty()?std::vector<Id>(g.nodes.size()):g.outputs;
    if (g.outputs.empty()) std::iota(seeds.begin(),seeds.end(),Id{0});
    std::vector<bool> seen(g.nodes.size());
    std::vector<Id> work, live;
    for (auto s:seeds) if (s<g.nodes.size()&&!seen[s]) { seen[s]=true; work.push_back(s); }
    while (!work.empty()) {
        // Walk backwards through inputs: a node is live if some live user needs it.
        auto n=work.back(); work.pop_back(); live.push_back(n);
        for (auto i:g.nodes[n].inputs) if (!seen[i]) { seen[i]=true; work.push_back(i); }
    }
    std::sort(live.begin(),live.end());
    return live;
}
// One directed cycle if present, else nullopt. Iterative DFS with colors.
inline std::optional<std::vector<Id>> find_cycle(const Graph& g) {
    if (!verify(g).empty()) return std::nullopt;
    enum class Mark : std::uint8_t { White, Gray, Black };
    std::vector<Mark> mark(g.nodes.size(),Mark::White);
    std::vector<Id> stack, iters(g.nodes.size());
    for (Id root=0;root<g.nodes.size();++root) if (mark[root]==Mark::White) {
        std::vector<std::pair<Id,std::size_t>> work{{root,0}};
        mark[root]=Mark::Gray; stack.push_back(root);
        while (!work.empty()) {
            auto& [n,k]=work.back();
            if (k<g.nodes[n].inputs.size()) {
                auto m=g.nodes[n].inputs[k++];
                if (mark[m]==Mark::Gray) {
                    auto first=std::find(stack.begin(),stack.end(),m);
                    std::vector<Id> cycle(first,stack.end()); cycle.push_back(m);
                    return cycle;
                }
                if (mark[m]==Mark::White) { mark[m]=Mark::Gray; stack.push_back(m); work.emplace_back(m,0); }
            } else {
                mark[n]=Mark::Black; stack.pop_back(); work.pop_back();
            }
        }
    }
    return std::nullopt;
}
} // namespace metachain::backend
