// MVBE-Clipper2: backend adapter scaffold.
// Keep this translation unit independent of the parser and consume validated MVIR.
#include <string>
#include <string_view>
namespace metavect::backend {
struct MVBE_Clipper2_descriptor {
    static constexpr std::string_view name = "Clipper2";
    static constexpr unsigned abi_version = 1;
    static constexpr bool implemented = false;
};
// Actual rendering belongs here once the backend dependency and output contract are selected.
std::string_view MVBE_Clipper2_status() noexcept {
    return "backend scaffold: emission not implemented";
}
}
