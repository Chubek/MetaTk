#include "metachain/ISched.hpp"
#include "satie/BackendSAT.hpp"

namespace metachain::isched {
Result constraint(const std::vector<Task>& tasks,Options o) {
    auto baseline=list_schedule(tasks,o); if (!baseline) return baseline;
    if (tasks.empty()) { baseline.optimal=true; return baseline; }
    if (baseline.makespan>128||tasks.size()>32) {
        Result r; r.diagnostics.push_back({"SAT scheduler supports at most 32 tasks and 128 cycles",0}); return r;
    }
    Result best=baseline;
    auto attempt=[&](std::size_t horizon)->std::optional<Result> {
        sat::Formula f; std::vector<std::vector<int>> vars(tasks.size());
        for (Id n=0;n<tasks.size();++n) {
            for (std::size_t c=0;c<horizon&&c<o.max_cycles&&c+tasks[n].latency<=horizon;++c) vars[n].push_back(f.fresh());
            f.exactly_one(vars[n]);
        }
        for (Id n=0;n<tasks.size();++n) for (auto d:tasks[n].dependencies)
            for (std::size_t p=0;p<vars[d.predecessor].size();++p)
                for (std::size_t c=0;c<vars[n].size()&&c<p+d.delay;++c) f.clauses.push_back({-vars[d.predecessor][p],-vars[n][c]});
        for (std::size_t c=0;c<horizon;++c) {
            std::vector<int> issue; std::map<std::string,std::vector<int>> resources;
            for (Id n=0;n<tasks.size();++n) if (c<vars[n].size()) { issue.push_back(vars[n][c]); resources[tasks[n].resource].push_back(vars[n][c]); }
            f.at_most(issue,o.issue_width);
            for (const auto& [resource,capacity]:o.resources) f.at_most(resources[resource],capacity);
        }
        auto model=sat::solve(f); if (!model.satisfiable) return std::nullopt;
        Result r; r.cycle.resize(tasks.size());
        // Baseline order is topological; stable cycle sorting retains zero-delay edges.
        r.order=baseline.order;
        for (Id n=0;n<tasks.size();++n) for (std::size_t c=0;c<vars[n].size();++c) if (model.values[vars[n][c]]) {
            r.cycle[n]=c; r.makespan=std::max(r.makespan,c+tasks[n].latency);
        }
        std::stable_sort(r.order.begin(),r.order.end(),[&](Id a,Id b){return r.cycle[a]<r.cycle[b];});
        return r;
    };
    try {
        std::size_t low=1,high=baseline.makespan;
        while (low<high) {
            auto middle=low+(high-low)/2; auto r=attempt(middle);
            if (r) { high=r->makespan; best=std::move(*r); } else low=middle+1;
        }
        best.optimal=true;
    } catch (const std::length_error& e) { best.diagnostics.push_back({e.what(),0}); }
    return best;
}
} // namespace metachain::isched
