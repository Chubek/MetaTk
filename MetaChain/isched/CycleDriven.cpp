#include "metachain/ISched.hpp"
namespace metachain::isched {
Result cycle_driven(const std::vector<Task>& tasks,Options o) { return list_schedule(tasks,o,false); }
} // namespace metachain::isched
