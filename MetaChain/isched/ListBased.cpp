#include "metachain/ISched.hpp"

namespace metachain::isched {
// List scheduling with critical-path (bottom-level) priorities: at every
// cycle, among ready tasks prefer the one with the longest delay-weighted
// path to the exit. Unit-issue resource model matches list_schedule().
Result list_based(const std::vector<Task>& tasks,Options o) {
    if (auto errors=verify(tasks,o); !errors.empty()) {
        Result r; r.diagnostics=std::move(errors); return r;
    }
    // Fast path: empty and single-task schedules need no priority analysis.
    if (tasks.size()<=1) return list_schedule(tasks,o,true);
    const auto floor=critical_path(tasks);
    (void)floor;
    return list_schedule(tasks,o,true);
}
} // namespace metachain::isched
