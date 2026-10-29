#pragma once
#include "doxtk/DoxTk.hpp"

// High-level conversion orchestration.
// This header is intentionally dependency-free. Add stable public declarations
// here and keep implementations in the matching src/DoxTkDriver.cpp.
namespace doxtk {
// Source/target formats must be explicit. All validation and serialization
// completes before an output file is opened (conversion failures do not truncate it).
[[nodiscard]] Result<std::vector<std::byte>> convert(
    std::span<const std::byte> input, const ConvertOptions& options);
[[nodiscard]] Error convert_file(const std::filesystem::path& input,
    const std::filesystem::path& output, const ConvertOptions& options);
// Returns the written path and successful conversion diagnostics. Like
// convert_file(), validates/serializes before opening the output; not atomic.
[[nodiscard]] Result<std::filesystem::path> convert_file_with_diagnostics(
    const std::filesystem::path& input, const std::filesystem::path& output,
    const ConvertOptions& options);

struct DriverModuleInfo {
    static constexpr std::string_view description = "High-level conversion orchestration.";
};
} // namespace doxtk
