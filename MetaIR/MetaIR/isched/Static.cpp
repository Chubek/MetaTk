#include "metachain/ISched.hpp"

namespace metachain::isched {
// Static (serial) scheduling: a deterministic single-issue topological order
// used as a baseline and as a fallback when parallel resources are unknown.
// Preserves the input task order wherever dependencies allow.
Result static_schedule(const std::vector<Task>& tasks,Options o) {
    if (auto errors=verify(tasks,o); !errors.empty()) {
        Result r; r.diagnostics=std::move(errors); return r;
    }
    if (tasks.empty()) return list_schedule(tasks,o,false);
    auto serial=o; serial.issue_width=1;
    serial.resources.clear();
    return list_schedule(tasks,serial,false);
}
} // namespace metachain::isched
