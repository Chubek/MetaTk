#pragma once
#include "Backend.hpp"
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <sstream>
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
inline std::vector<Task> dependencies(const std::vector<metail::Instruction>& code) {    std::vector<Task> tasks(code.size()); std::vector<backend::Access> accesses;
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
struct ScheduleStats {
    std::size_t tasks=0, makespan=0;
    std::map<std::size_t,unsigned> per_cycle;
    std::map<std::string,std::size_t> per_resource;
};
inline ScheduleStats schedule_stats(const std::vector<Task>& tasks,const Result& r) {
    ScheduleStats s; s.tasks=tasks.size(); s.makespan=r.makespan;
    for (auto n:r.order) {
        if (n>=tasks.size()||n>=r.cycle.size()) break;
        ++s.per_cycle[r.cycle[n]];
        ++s.per_resource[tasks[n].resource];
    }
    return s;
}
inline std::string dump_schedule(const std::vector<Task>& tasks,const Result& r) {
    std::ostringstream out;
    out << "schedule makespan=" << r.makespan << (r.optimal?" optimal":"") << " tasks=" << tasks.size() << "\n";
    for (auto n:r.order) {
        if (n>=tasks.size()||n>=r.cycle.size()) break;
        out << "  cycle " << r.cycle[n] << ": t" << n << " lat=" << tasks[n].latency;
        if (!tasks[n].resource.empty()) out << " res=" << tasks[n].resource;
        if (!tasks[n].dependencies.empty()) {
            out << " after[";
            for (std::size_t k=0;k<tasks[n].dependencies.size();++k)
                out << (k?",":"") << "t" << tasks[n].dependencies[k].predecessor << "+" << tasks[n].dependencies[k].delay;
            out << "]";
        }
        out << "\n";
    }
    for (const auto& d:r.diagnostics) out << "  diagnostic: " << d.message << " @t" << d.node << "\n";
    return out.str();
}
// Length of the longest delay-weighted dependency chain (issue-cycle lower bound).
inline std::size_t critical_path(const std::vector<Task>& tasks) {
    backend::Graph g; g.nodes.resize(tasks.size());
    for (Id n=0;n<tasks.size();++n) { g.nodes[n].opcode="task"; for (auto d:tasks[n].dependencies) g.nodes[n].inputs.push_back(d.predecessor); }
    auto topo=backend::topological_order(g);
    if (!topo) return 0;
    const auto infinite=std::numeric_limits<std::size_t>::max();
    std::vector<std::size_t> longest(tasks.size());
    for (auto n:*topo) {
        // Completion bound: cycle[n] >= cycle[p]+delay, so completion[n] >=
        // completion[p]-latency[p]+delay+latency[n]. Delays already include
        // the producer latency for value edges; do not count it twice.
        std::size_t best=tasks[n].latency;
        for (auto d:tasks[n].dependencies) {
            const auto pred_start=longest[d.predecessor]>=tasks[d.predecessor].latency
                ? longest[d.predecessor]-tasks[d.predecessor].latency : std::size_t{0};
            const auto via=pred_start>=infinite-d.delay-tasks[n].latency
                ? infinite : pred_start+std::size_t(d.delay)+tasks[n].latency;
            best=std::max(best,via);
        }
        longest[n]=best;
    }
    std::size_t overall=0;
    for (auto v:longest) overall=std::max(overall,v);
    return overall;
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
