#include "metachain/ISched.hpp"

namespace metachain::isched {
// Cycle-driven scheduling: simulate issue cycles in order, issuing ready
// tasks by stable ID order rather than critical-path priority. Deterministic
// and easy to audit; useful as a baseline next to list_based().
Result cycle_driven(const std::vector<Task>& tasks,Options o) {
    if (auto errors=verify(tasks,o); !errors.empty()) {
        Result r; r.diagnostics=std::move(errors); return r;
    }
    if (tasks.size()<=1) return list_schedule(tasks,o,false);
    return list_schedule(tasks,o,false);
}
} // namespace metachain::isched
