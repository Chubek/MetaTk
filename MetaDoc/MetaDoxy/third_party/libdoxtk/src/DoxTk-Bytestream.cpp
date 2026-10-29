#include "doxtk/DoxTk-Bytestream.hpp"

namespace doxtk {
std::vector<std::byte> to_bytes(std::string_view text) {
    std::vector<std::byte> bytes;
    bytes.reserve(text.size());
    for (unsigned char ch : text) bytes.push_back(static_cast<std::byte>(ch));
    return bytes;
}

std::string bytes_to_string(std::span<const std::byte> bytes) {
    std::string text;
    text.reserve(bytes.size());
    for (std::byte ch : bytes) text.push_back(static_cast<char>(std::to_integer<unsigned char>(ch)));
    return text;
}
} // namespace doxtk
