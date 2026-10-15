#include "metachain/ISched.hpp"
namespace metachain::isched {
Result list_based(const std::vector<Task>& tasks,Options o) { return list_schedule(tasks,o,true); }
} // namespace metachain::isched
