#include "metachain/ISched.hpp"
namespace metachain::isched {
Result static_schedule(const std::vector<Task>& tasks,Options o) {
    Result r; r.diagnostics=verify(tasks,o); if (!r) return r;
    // Stable topological serial schedule: a useful deterministic baseline.
    auto serial=o; serial.issue_width=1;
    return list_schedule(tasks,serial,false);
}
} // namespace metachain::isched
