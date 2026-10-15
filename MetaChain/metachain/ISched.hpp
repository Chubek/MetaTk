#pragma once
#include "Backend.hpp"
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace metachain::isched {
using backend::Id;
struct Dependency { Id predecessor=0; unsigned delay=1; };
struct Task {
    std::vector<Dependency> dependencies;
    unsigned latency=1;
    std::string resource;
    bool speculatable=false;
};
struct Options {
    unsigned issue_width=1;
    std::map<std::string,unsigned> resources; // per-cycle issue capacities
    std::size_t max_cycles=1000000;
};
struct Result {
    std::vector<std::size_t> cycle;
    std::vector<Id> order;
    std::size_t makespan=0;
    bool optimal=false;
    std::vector<backend::Diagnostic> diagnostics;
    explicit operator bool() const { return diagnostics.empty(); }
};
inline std::vector<backend::Diagnostic> verify(const std::vector<Task>& tasks,const Options& o) {
    std::vector<backend::Diagnostic> errors;
    if (!o.issue_width || !o.max_cycles) errors.push_back({"issue width and cycle limit must be positive",0});
    backend::Graph g; g.nodes.resize(tasks.size());
    for (Id n=0;n<tasks.size();++n) {
        g.nodes[n].opcode="task";
        if (!tasks[n].latency) errors.push_back({"task latency must be positive",n});
        auto cap=o.resources.find(tasks[n].resource);
        if (cap!=o.resources.end()&&!cap->second) errors.push_back({"resource has zero capacity",n});
        for (const auto& d:tasks[n].dependencies) {
            if (d.predecessor>=tasks.size()) errors.push_back({"dependency outside task list",n});
            g.nodes[n].inputs.push_back(d.predecessor);
        }
    }
    if (errors.empty()&&!backend::topological_order(g)) errors.push_back({"cyclic scheduling dependencies",0});
    return errors;
}
// Unit-occupancy resource model: latency delays results, not resource reuse.
inline Result list_schedule(const std::vector<Task>& tasks,Options o={},bool critical=true) {
    Result r; r.diagnostics=verify(tasks,o); if (!r) return r;
    backend::Graph g; g.nodes.resize(tasks.size());
    for (Id n=0;n<tasks.size();++n) { g.nodes[n].opcode="task"; for (auto d:tasks[n].dependencies) g.nodes[n].inputs.push_back(d.predecessor); }
    auto topo=*backend::topological_order(g);
    std::vector<std::size_t> height(tasks.size());
    const auto infinite=std::numeric_limits<std::size_t>::max();
    for (auto n:topo) height[n]=tasks[n].latency;
    for (auto it=topo.rbegin();it!=topo.rend();++it)
        for (auto d:tasks[*it].dependencies) {
            const auto increment=d.delay>=infinite-height[*it] ? infinite : std::size_t(d.delay)+height[*it];
            height[d.predecessor]=std::max(height[d.predecessor],increment);
        }
    const auto unscheduled=std::numeric_limits<std::size_t>::max();
    r.cycle.assign(tasks.size(),unscheduled);
    for (std::size_t cycle=0;r.order.size()<tasks.size();++cycle) {
        if (cycle>=o.max_cycles) { r.diagnostics.push_back({"scheduling cycle limit exceeded",0}); return r; }
        std::map<std::string,unsigned> used; unsigned issued=0;
        // Repeat so zero-delay dependencies can issue in the same cycle.
        while (issued<o.issue_width) {
            Id best=tasks.size();
            for (Id n=0;n<tasks.size();++n) if (r.cycle[n]==unscheduled) {
                bool ready=true;
                for (auto d:tasks[n].dependencies)
                    if (r.cycle[d.predecessor]==unscheduled || cycle<r.cycle[d.predecessor]+d.delay) ready=false;
                auto cap=o.resources.find(tasks[n].resource);
                if (cap!=o.resources.end()&&used[tasks[n].resource]>=cap->second) ready=false;
                if (ready&&(best==tasks.size() || (critical&&height[n]>height[best]))) best=n;
            }
            if (best==tasks.size()) break;
            r.cycle[best]=cycle; r.order.push_back(best); ++used[tasks[best].resource]; ++issued;
            r.makespan=std::max(r.makespan,cycle+tasks[best].latency);
        }
    }
    return r;
}
inline bool valid(const std::vector<Task>& tasks,const Result& r,Options o={}) {
    if (!r || !verify(tasks,o).empty() || r.cycle.size()!=tasks.size() || r.order.size()!=tasks.size()) return false;
    std::set<Id> seen;
    std::map<std::size_t,unsigned> issued;
    std::map<std::pair<std::size_t,std::string>,unsigned> used;
    std::size_t makespan=0;
    for (auto n:r.order) {
        if (n>=tasks.size()||!seen.insert(n).second||r.cycle[n]>=o.max_cycles) return false;
        if (++issued[r.cycle[n]]>o.issue_width) return false;
        auto cap=o.resources.find(tasks[n].resource);
        if (cap!=o.resources.end()&&++used[{r.cycle[n],tasks[n].resource}]>cap->second) return false;
        for (auto d:tasks[n].dependencies)
            if (!seen.count(d.predecessor)||r.cycle[n]<r.cycle[d.predecessor]+d.delay) return false;
        makespan=std::max(makespan,r.cycle[n]+tasks[n].latency);
    }
    return makespan==r.makespan;
}
inline std::vector<Task> dependencies(const std::vector<metail::Instruction>& code) {
    std::vector<Task> tasks(code.size()); std::vector<backend::Access> accesses;
    for (const auto& i:code) accesses.push_back(backend::access(i));
    for (Id n=0;n<code.size();++n) {
        auto& t=tasks[n]; const auto& b=accesses[n]; t.speculatable=!b.barrier&&!b.may_trap;
        for (Id p=0;p<n;++p) {
            const auto& a=accesses[p];
            if (a.barrier||b.barrier||a.may_trap||b.may_trap||backend::intersects(a.defs,b.uses)||backend::intersects(a.uses,b.defs)||backend::intersects(a.defs,b.defs))
                t.dependencies.push_back({p,1});
        }
    }
    return tasks;
}
Result list_based(const std::vector<Task>&,Options={});
Result cycle_driven(const std::vector<Task>&,Options={});
Result static_schedule(const std::vector<Task>&,Options={});
Result constraint(const std::vector<Task>&,Options={});
// Control dependencies may only be relaxed for explicitly speculatable tasks.
struct ControlDependency { Id task=0, predecessor=0; };
Result speculative(std::vector<Task>,const std::vector<ControlDependency>&,Options={});
struct TraceBlock { std::vector<Id> successors; std::vector<double> weights; };
std::vector<std::vector<Id>> trace_schedule(const std::vector<TraceBlock>&);
struct DelayInstruction { backend::Access access; bool branch=false, nop=false; };
struct DelayResult {
    // nullopt represents a nop. Input must reserve `slots` nops after branches.
    std::vector<std::optional<Id>> order;
    std::vector<backend::Diagnostic> diagnostics;
    explicit operator bool() const { return diagnostics.empty(); }
};
DelayResult delay_slots(const std::vector<DelayInstruction>&,unsigned slots=1);
} // namespace metachain::isched
