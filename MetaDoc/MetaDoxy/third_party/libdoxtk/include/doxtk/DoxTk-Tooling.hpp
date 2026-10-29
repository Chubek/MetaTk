#pragma once
#include "doxtk/DoxTk.hpp"

// Registries and shared toolchain utilities.
// This header is intentionally dependency-free. Add stable public declarations
// here and keep implementations in the matching src/DoxTkTooling.cpp.
namespace doxtk {
struct FormatInfo {
    std::string_view name;
    std::string_view ir;
    std::string_view media_type;
    bool readable;
    bool writable;
};

// Immutable built-ins: format names are distinct from IR identifiers.
[[nodiscard]] std::span<const FormatInfo> builtin_formats() noexcept;
[[nodiscard]] const FormatInfo* find_format(std::string_view name) noexcept;

struct ToolingModuleInfo {
    static constexpr std::string_view description = "Registries and shared toolchain utilities.";
};
} // namespace doxtk
