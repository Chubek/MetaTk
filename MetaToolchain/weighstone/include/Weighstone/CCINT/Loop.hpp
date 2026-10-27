#pragma once
#include "Affine.hpp"
#include "Graph.hpp"
#include "Recurrence.hpp"

namespace weighstone::ccint {
// Half-open counted loop. Positive steps test value < stop, negative steps > stop.
// Values and trip counts are mathematical integers, not wrapping machine values.
class LoopRange {
    BigInt start_,stop_,step_;
public:
    LoopRange(BigInt start,BigInt stop,BigInt step=1)
        : start_(std::move(start)),stop_(std::move(stop)),step_(std::move(step)) {
        if(step_==0) throw std::invalid_argument("zero loop step");
    }
    const BigInt& start() const noexcept { return start_; }
    const BigInt& stop() const noexcept { return stop_; }
    const BigInt& step() const noexcept { return step_; }
    BigInt trip_count() const {
        const AffineRecurrence rec{start_,step_};
        return step_>0 ? *rec.trip_count_less_than(stop_) : *rec.trip_count_greater_than(stop_);
    }
    BigInt at(const BigInt& ordinal) const {
        if(ordinal<0 || ordinal>=trip_count()) throw std::out_of_range("loop ordinal outside range");
        return start_+step_*ordinal;
    }
    BigInt tile_count(const BigInt& tile_size) const {
        if(tile_size<=0) throw std::invalid_argument("nonpositive tile size");
        return ceil_div(trip_count(),tile_size);
    }
    LoopRange tile(const BigInt& tile_ordinal,const BigInt& tile_size) const {
        if(tile_ordinal<0 || tile_ordinal>=tile_count(tile_size)) throw std::out_of_range("tile ordinal outside loop");
        const BigInt first=tile_ordinal*tile_size;
        const BigInt count=std::min(tile_size,BigInt(trip_count()-first));
        const BigInt begin=start_+step_*first;
        return {begin,BigInt(begin+step_*count),step_};
    }
};

// Rectangular nests use increasing ordinal coordinates, even for descending or
// strided induction values. This makes lexicographic execution order explicit.
class LoopNest {
    std::vector<std::string> ordinals_;
    std::vector<LoopRange> loops_;
public:
    LoopNest(std::vector<std::string> ordinals,std::vector<LoopRange> loops)
        : ordinals_(std::move(ordinals)),loops_(std::move(loops)) {
        if(ordinals_.size()!=loops_.size()) throw std::invalid_argument("loop nest rank mismatch");
        const Polyhedron validate_names(ordinals_);
    }
    Polyhedron domain() const {
        Polyhedron result(ordinals_);
        for(std::size_t i=0;i<loops_.size();++i) {
            const auto ordinal=LinearExpr::variable(ordinals_[i]);
            result.add({ordinal,Relation::greater_equal,0});
            result.add({ordinal,Relation::less_equal,BigInt(loops_[i].trip_count()-1)});
        }
        return result;
    }
    AffineMap induction_values() const {
        std::vector<LinearExpr> values;
        for(std::size_t i=0;i<loops_.size();++i)
            values.push_back(LinearExpr::variable(ordinals_[i],loops_[i].step())+LinearExpr(loops_[i].start()));
        return {ordinals_,std::move(values)};
    }
};

struct AffineAccess {
    // Distinct nonempty objects MUST be known nonaliasing. Equal objects use the
    // same element-coordinate system. Flatten aliased views with linearize().
    std::string object;
    AffineMap subscript;
    bool write=false;
};
struct DependenceBudget {
    AnalysisBudget domain;
    std::size_t max_pairs=1000000;
};
struct DependenceAnalysis {
    Verdict complete;
    // Never expose a partial graph as if missing edges proved independence.
    std::optional<DependenceGraph> graph;
};

// Exact bounded analysis for one ordered list of accesses per domain point.
// Points execute lexicographically in domain.variables() order, accesses in list
// order. All conflicting flow/anti/output pairs are retained (conservative, not
// just nearest dependences). Non-memory effects must be supplied separately.
inline DependenceAnalysis analyze_dependences(const Polyhedron& domain,
        const std::vector<AffineAccess>& accesses,DependenceBudget budget={}) {
    std::map<std::string,std::size_t> ranks;
    for(const auto& access:accesses) {
        if(access.subscript.inputs()!=domain.variables()) throw std::invalid_argument("access coordinate mismatch");
        if(access.object.empty()) return {{Decision::unknown,"unresolved alias identity"},std::nullopt};
        const auto [it,inserted]=ranks.emplace(access.object,access.subscript.outputs().size());
        if(!inserted && it->second!=access.subscript.outputs().size())
            throw std::invalid_argument("inconsistent coordinates for memory object");
    }
    DependenceGraph graph(accesses.size(),domain.variables().size());
    if(accesses.empty()) return {{Decision::proven,"no memory accesses"},std::move(graph)};
    const auto points=domain.integer_points(budget.domain);
    if(!points.complete.is_proven()) return {points.complete,std::nullopt};
    const BigInt events=BigInt(points.points.size())*accesses.size();
    if(events*(events-1)/2>budget.max_pairs)
        return {{Decision::unknown,"dependence pair budget exhausted"},std::nullopt};
    struct Event {std::size_t point,access; std::vector<BigInt> address;};
    std::vector<Event> ordered;
    for(std::size_t p=0;p<points.points.size();++p)
        for(std::size_t a=0;a<accesses.size();++a)
            ordered.push_back({p,a,accesses[a].subscript.eval(points.points[p])});
    for(std::size_t i=0;i<ordered.size();++i) for(std::size_t j=i+1;j<ordered.size();++j) {
        const auto& from=ordered[i]; const auto& to=ordered[j];
        const auto& a=accesses[from.access]; const auto& b=accesses[to.access];
        if((!a.write && !b.write) || a.object!=b.object || from.address!=to.address) continue;
        std::vector<BigInt> distance;
        for(const auto& v:domain.variables())
            distance.push_back(BigInt(points.points[to.point].at(v)-points.points[from.point].at(v)));
        graph.add_edge(from.access,to.access,std::move(distance));
    }
    return {{Decision::proven,"complete bounded memory dependence analysis"},std::move(graph)};
}
}
