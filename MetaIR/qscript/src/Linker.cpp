#include "qscript/QScript.h"
#include "qscript/src/Runtime.h"
namespace qscript {
Result<std::vector<std::uint8_t>> link(const std::vector<std::vector<std::uint8_t>>& objects,Options o) {
    const auto& limits = o.toolchain.linker;
    if (objects.empty() || objects.size() > limits.max_objects) return metachain::Toolchain(o.toolchain).link(objects);
    std::uint64_t total = 0;
    for (const auto& object : objects) {
        if (object.size() > limits.max_input_size || object.size() > limits.max_total_input_size - std::min(total, limits.max_total_input_size))
            return metachain::Toolchain(o.toolchain).link(objects);
        total += object.size();
    }
    for (const auto& object : objects)
        if (detail::native_runtime_object(object)) return detail::link_native_runtime(objects, o);
    return metachain::Toolchain(o.toolchain).link(objects);
}
} // namespace qscript
