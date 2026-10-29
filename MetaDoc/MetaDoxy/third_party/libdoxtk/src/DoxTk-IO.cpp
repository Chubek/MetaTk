#include "doxtk/DoxTk-IO.hpp"
#include <algorithm>
#include <array>
#include <fstream>

namespace doxtk {
Result<std::vector<std::byte>> read_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return Result<std::vector<std::byte>>::failure(
        ErrorCode::io_error, "Unable to open input file", path.string());
    std::vector<std::byte> bytes;
    std::array<char, 8192> buffer{};
    while (input.read(buffer.data(), static_cast<std::streamsize>(buffer.size())) || input.gcount() > 0) {
        for (std::streamsize i = 0; i < input.gcount(); ++i) {
            bytes.push_back(static_cast<std::byte>(static_cast<unsigned char>(buffer[static_cast<std::size_t>(i)])));
        }
    }
    if (input.bad() || !input.eof()) return Result<std::vector<std::byte>>::failure(
        ErrorCode::io_error, "Failed while reading input file", path.string());
    return Result<std::vector<std::byte>>::success(std::move(bytes));
}

Error write_file(const std::filesystem::path& path, std::span<const std::byte> bytes) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) return {ErrorCode::io_error, "Unable to open output file", path.string()};
    std::array<char, 8192> buffer{};
    for (std::size_t offset = 0; offset < bytes.size();) {
        const auto count = std::min(buffer.size(), bytes.size() - offset);
        for (std::size_t i = 0; i < count; ++i) {
            buffer[i] = static_cast<char>(std::to_integer<unsigned char>(bytes[offset + i]));
        }
        output.write(buffer.data(), static_cast<std::streamsize>(count));
        if (!output) return {ErrorCode::io_error, "Failed while writing output file", path.string()};
        offset += count;
    }
    output.close(); // Includes flushing buffered bytes; delayed errors matter.
    if (!output) return {ErrorCode::io_error, "Failed while closing output file", path.string()};
    return {};
}
} // namespace doxtk
