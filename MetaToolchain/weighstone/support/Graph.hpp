#pragma once
#include <algorithm>
#include <cstddef>
#include <functional>
#include <optional>
#include <queue>
#include <stdexcept>
#include <utility>
#include <vector>

namespace weighstone::support {
// Stable dense vertex IDs; deterministic traversal; duplicate edges are ignored.
class DirectedGraph {
    std::vector<std::vector<std::size_t>> edges_;
    void validate(std::size_t vertex) const {
        if(vertex>=edges_.size()) throw std::out_of_range("graph vertex outside graph");
    }
public:
    explicit DirectedGraph(std::size_t vertices=0) : edges_(vertices) {}
    std::size_t size() const noexcept { return edges_.size(); }
    std::size_t add_vertex() { edges_.emplace_back(); return edges_.size()-1; }
    const auto& successors(std::size_t vertex) const { validate(vertex); return edges_[vertex]; }
    void add_edge(std::size_t from,std::size_t to) {
        validate(from); validate(to);
        auto& edges=edges_[from];
        const auto it=std::lower_bound(edges.begin(),edges.end(),to);
        if(it==edges.end() || *it!=to) edges.insert(it,to);
    }
    bool reachable(std::size_t from,std::size_t to) const {
        validate(from); validate(to);
        std::vector<bool> seen(size(),false);
        std::vector<std::size_t> pending{from}; seen[from]=true;
        while(!pending.empty()) {
            auto v=pending.back(); pending.pop_back();
            if(v==to) return true; // includes the zero-edge path
            for(auto w:edges_[v]) if(!seen[w]) {seen[w]=true; pending.push_back(w);}
        }
        return false;
    }
    // nullopt for any cycle, including a self edge; empty graph has empty order.
    std::optional<std::vector<std::size_t>> topological_order() const {
        std::vector<std::size_t> indegree(size(),0),result;
        for(const auto& edges:edges_) for(auto to:edges) ++indegree[to];
        std::priority_queue<std::size_t,std::vector<std::size_t>,std::greater<std::size_t>> ready;
        for(std::size_t v=0;v<size();++v) if(indegree[v]==0) ready.push(v);
        while(!ready.empty()) {
            auto v=ready.top(); ready.pop(); result.push_back(v);
            for(auto w:edges_[v]) if(--indegree[w]==0) ready.push(w);
        }
        if(result.size()!=size()) return std::nullopt;
        return result;
    }
    // Iterative Kosaraju: O(V+E) traversal, no call-stack growth on deep CFGs.
    // Members are sorted, components are in topological order of condensation.
    std::vector<std::vector<std::size_t>> strongly_connected_components() const {
        std::vector<bool> seen(size(),false);
        std::vector<std::size_t> finish;
        for(std::size_t root=0;root<size();++root) if(!seen[root]) {
            std::vector<std::pair<std::size_t,std::size_t>> stack{{root,0}}; seen[root]=true;
            while(!stack.empty()) {
                auto& [v,index]=stack.back();
                if(index==edges_[v].size()) {finish.push_back(v); stack.pop_back(); continue;}
                const auto w=edges_[v][index++];
                if(!seen[w]) {seen[w]=true; stack.emplace_back(w,0);}
            }
        }
        std::vector<std::vector<std::size_t>> reverse(size()),result;
        for(std::size_t v=0;v<size();++v) for(auto w:edges_[v]) reverse[w].push_back(v);
        std::fill(seen.begin(),seen.end(),false);
        for(auto it=finish.rbegin();it!=finish.rend();++it) if(!seen[*it]) {
            std::vector<std::size_t> stack{*it},component; seen[*it]=true;
            while(!stack.empty()) {
                const auto v=stack.back(); stack.pop_back(); component.push_back(v);
                for(auto w:reverse[v]) if(!seen[w]) {seen[w]=true; stack.push_back(w);}
            }
            std::sort(component.begin(),component.end()); result.push_back(std::move(component));
        }
        return result;
    }
};
}
