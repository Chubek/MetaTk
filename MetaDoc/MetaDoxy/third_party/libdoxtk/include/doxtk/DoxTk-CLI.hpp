#pragma once
#include "doxtk/DoxTk.hpp"

// Reusable command-line argument handling.
// This header is intentionally dependency-free. Add stable public declarations
// here and keep implementations in the matching src/DoxTkCLI.cpp.
namespace doxtk {
struct CLIOptions {
    bool help{false};
    bool show_version{false};
    bool list_formats{false};
    ConvertOptions conversion;
    std::filesystem::path input;
    std::optional<std::filesystem::path> output;
};

[[nodiscard]] Result<CLIOptions> parse_cli(std::span<const std::string_view> arguments,
    std::string_view fixed_target = {});
// Exit codes: 0 success, 1 I/O/conversion failure, 2 argument error.
int run_conversion_cli(int argc, char** argv, std::string_view name,
    std::string_view fixed_target = {});

struct CLIModuleInfo {
    static constexpr std::string_view description = "Reusable command-line argument handling.";
};
} // namespace doxtk
