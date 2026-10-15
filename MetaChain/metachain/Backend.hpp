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
} // namespace metachain::backend
