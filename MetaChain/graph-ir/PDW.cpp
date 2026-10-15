#include "GraphIR.hpp"

namespace metachain::graph {
std::vector<std::vector<Id>> pdw(const Graph& g) {
    if (!backend::verify(g).empty()) throw std::invalid_argument("invalid dependence graph");
    std::vector<std::vector<Id>> edges(g.nodes.size()),components;
    for (Id n=0;n<g.nodes.size();++n) for (auto i:g.nodes[n].inputs) { edges[n].push_back(i); edges[i].push_back(n); }
    std::vector<bool> visited(g.nodes.size());
    for (Id n=0;n<g.nodes.size();++n) if (!visited[n]) {
        std::vector<Id> component,work{n}; visited[n]=true;
        while (!work.empty()) { auto v=work.back(); work.pop_back(); component.push_back(v); for (auto u:edges[v]) if (!visited[u]) { visited[u]=true; work.push_back(u); } }
        std::sort(component.begin(),component.end()); components.push_back(std::move(component));
    }
    return components;
}
} // namespace metachain::graph
