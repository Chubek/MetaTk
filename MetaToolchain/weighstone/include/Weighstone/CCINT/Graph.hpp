#pragma once
#include "Constraints.hpp"
#include "../../../support/Graph.hpp"
#include "../../../support/Permutation.hpp"

namespace weighstone::ccint {
// Statement-level graph with iteration-distance facts. The caller must include
// every flow, anti, output and other ordering dependence. No alias inference.
class DependenceGraph {
public:
    struct Edge {
        std::size_t source,target;
        std::optional<std::vector<BigInt>> distance; // target iteration - source
    };
private:
    weighstone::support::DirectedGraph graph_;
    std::size_t depth_;
    std::vector<Edge> edges_;
public:
    DependenceGraph(std::size_t statements,std::size_t loop_depth) : graph_(statements),depth_(loop_depth) {}
    const auto& graph() const noexcept { return graph_; }
    const auto& edges() const noexcept { return edges_; }
    std::size_t loop_depth() const noexcept { return depth_; }
    void add_edge(std::size_t source,std::size_t target,std::optional<std::vector<BigInt>> distance=std::nullopt) {
        if(distance && distance->size()!=depth_) throw std::invalid_argument("dependence distance rank mismatch");
        graph_.add_edge(source,target);
        edges_.push_back({source,target,std::move(distance)});
    }
    // Preserve lexicographic order of every distance. Zero-distance edges keep
    // their original within-iteration statement order (not changed by this API).
    Verdict permutation_legal(const std::vector<std::size_t>& order) const {
        weighstone::support::validate_permutation(order,depth_);
        bool unknown=false;
        for(const auto& edge:edges_) {
            if(!edge.distance) {unknown=true; continue;}
            for(auto axis:order) {
                const auto& d=(*edge.distance)[axis];
                if(d<0) return {Decision::disproven,"permutation reverses a dependence"};
                if(d>0) break;
            }
        }
        return unknown ? Verdict{Decision::unknown,"unresolved dependence distance"}
                       : Verdict{Decision::proven,"all dependence directions preserved"};
    }
    // Axis can run concurrently inside each fixed outer prefix iff no edge is
    // carried by this axis. Edges already carried by an outer axis are irrelevant.
    Verdict parallel_axis(std::size_t axis) const {
        if(axis>=depth_) throw std::out_of_range("loop axis outside nest");
        bool unknown=false;
        for(const auto& edge:edges_) {
            if(!edge.distance) {unknown=true; continue;}
            bool outer=false;
            for(std::size_t i=0;i<axis;++i) outer|=(*edge.distance)[i]!=0;
            if(!outer && (*edge.distance)[axis]!=0) return {Decision::disproven,"loop-carried dependence"};
        }
        return unknown ? Verdict{Decision::unknown,"unresolved dependence distance"}
                       : Verdict{Decision::proven,"no dependence carried by this axis"};
    }
};
}
