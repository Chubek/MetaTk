#include "qscript/QScript.h"
namespace qscript {
Result<std::vector<std::uint8_t>> link(const std::vector<std::vector<std::uint8_t>>& objects,Options o) {
    return metachain::Toolchain(o.toolchain).link(objects);
}
} // namespace qscript
