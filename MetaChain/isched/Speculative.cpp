#include "metachain/ISched.hpp"
namespace metachain::isched {
Result speculative(std::vector<Task> tasks,const std::vector<ControlDependency>& control,Options o) {
    Result r; r.diagnostics=verify(tasks,o); if (!r) return r;
    for (auto edge:control) {
        if (edge.task>=tasks.size()||edge.predecessor>=tasks.size()) { r.diagnostics.push_back({"invalid control dependency",edge.task}); return r; }
        if (!tasks[edge.task].speculatable) { r.diagnostics.push_back({"cannot speculate an effectful or trapping task",edge.task}); return r; }
        auto& deps=tasks[edge.task].dependencies;
        deps.erase(std::remove_if(deps.begin(),deps.end(),[&](auto d){return d.predecessor==edge.predecessor;}),deps.end());
    }
    return list_schedule(tasks,o);
}
} // namespace metachain::isched
