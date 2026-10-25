// Native implementation boundary for the MetaVect `heap` standard module.
// Keep exported operations small, checked, and independent of renderer internals.
#include <cstddef>
#include <string_view>
namespace metavect::stdlib {
struct module_descriptor {
    std::string_view name;
    unsigned abi_version;
    bool native_implementation_available;
};
const module_descriptor heap_module{"heap", 1u, false};
// The VM should report an explicit unsupported-operation diagnostic until this
// module's native functions are registered; do not silently return fabricated values.
}
