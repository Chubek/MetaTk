#include "Driver.hpp"
#include <stdexcept>
namespace metatex::driver {
std::filesystem::path defaultOutput(const std::filesystem::path& input, std::string_view target) {
    auto result = input; result.replace_extension(findDevice(target).extension); return result;
}
void checkOutputPath(const std::filesystem::path& input, const std::filesystem::path& output) {
    if (input.empty() || output.empty()) return;
    if (std::filesystem::weakly_canonical(input) == std::filesystem::weakly_canonical(output) ||
        (std::filesystem::exists(input) && std::filesystem::exists(output) && std::filesystem::equivalent(input, output)))
        throw std::runtime_error("output must not overwrite the input file");
}
}
