#pragma once
#include <chrono>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>
namespace metatex::command {
struct ProcessOptions {
    int descriptor = 1;
    std::chrono::milliseconds timeout{30000};
    std::size_t outputLimit = 4 * 1024 * 1024;
};
// Arguments go directly to exec, without shell interpretation. Captured bytes
// are bounded; timeout/nonzero exit throws and terminates the process group.
std::string runProcess(const std::filesystem::path& executable,
    const std::vector<std::string>& arguments = {}, const ProcessOptions& options = {});
// Explicit shell evaluation. arguments become $1, $2, ...; $0 is "metatex".
std::string runShell(std::string_view script, const std::vector<std::string>& arguments = {},
    const ProcessOptions& options = {});
}
