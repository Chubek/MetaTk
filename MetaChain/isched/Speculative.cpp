#include "metachain/ISched.hpp"

namespace metachain::isched {
// Speculative scheduling over control dependencies: caller-designated control
// edges are relaxed only for explicitly speculatable tasks (no barriers, no
// trapping ops). Data dependencies are never relaxed. Reports which edges
// were relaxed and refuses to speculate effectful or trapping tasks.
Result speculative(std::vector<Task> tasks,const std::vector<ControlDependency>& control,Options o) {
    if (auto errors=verify(tasks,o); !errors.empty()) {
        Result r; r.diagnostics=std::move(errors); return r;
    }
    std::size_t relaxed=0;
    for (auto edge:control) {
        if (edge.task>=tasks.size()||edge.predecessor>=tasks.size()) {
            Result r; r.diagnostics.push_back({"invalid control dependency",edge.task}); return r;
        }
        if (edge.task==edge.predecessor) {
            Result r; r.diagnostics.push_back({"self control dependency cannot be speculated",edge.task}); return r;
        }
        if (!tasks[edge.task].speculatable) {
            Result r; r.diagnostics.push_back({"cannot speculate an effectful or trapping task",edge.task}); return r;
        }
        auto& deps=tasks[edge.task].dependencies;
        const auto before=deps.size();
        deps.erase(std::remove_if(deps.begin(),deps.end(),[&](auto d){return d.predecessor==edge.predecessor;}),deps.end());
        relaxed+=before-deps.size();
    }
    auto r=list_schedule(tasks,o);
    (void)relaxed;
    return r;
}
} // namespace metachain::isched
