#include "doxtk/DoxTk-Encoding.hpp"
#include "doxtk/DoxTk-Bytestream.hpp"
#include <cstdint>

namespace doxtk {
Error validate_utf8(std::string_view text) {
    for (std::size_t i = 0; i < text.size();) {
        const auto start = i;
        const auto lead = static_cast<unsigned char>(text[i++]);
        if (lead < 0x80) continue;
        std::size_t count = 0;
        std::uint32_t scalar = 0;
        std::uint32_t minimum = 0;
        if (lead >= 0xc2 && lead <= 0xdf) {
            count = 1; scalar = lead & 0x1f; minimum = 0x80;
        } else if (lead >= 0xe0 && lead <= 0xef) {
            count = 2; scalar = lead & 0x0f; minimum = 0x800;
        } else if (lead >= 0xf0 && lead <= 0xf4) {
            count = 3; scalar = lead & 0x07; minimum = 0x10000;
        } else {
            return {ErrorCode::parse_error, "Invalid UTF-8 leading byte", "utf-8 byte " + std::to_string(start)};
        }
        if (count > text.size() - i) {
            return {ErrorCode::parse_error, "Truncated UTF-8 sequence", "utf-8 byte " + std::to_string(start)};
        }
        for (std::size_t n = 0; n < count; ++n) {
            const auto ch = static_cast<unsigned char>(text[i++]);
            if ((ch & 0xc0) != 0x80) {
                return {ErrorCode::parse_error, "Invalid UTF-8 continuation byte", "utf-8 byte " + std::to_string(i - 1)};
            }
            scalar = (scalar << 6) | (ch & 0x3f);
        }
        if (scalar < minimum || scalar > 0x10ffff || (scalar >= 0xd800 && scalar <= 0xdfff)) {
            return {ErrorCode::parse_error, "Invalid UTF-8 scalar value", "utf-8 byte " + std::to_string(start)};
        }
    }
    return {};
}

Result<std::string> decode_text(std::span<const std::byte> bytes, std::string_view encoding) {
    if (encoding != "utf-8" && encoding != "ascii" && encoding != "iso-8859-1") {
        return Result<std::string>::failure(ErrorCode::invalid_argument,
            "Unsupported encoding; use utf-8, ascii, or iso-8859-1", std::string(encoding));
    }
    auto text = bytes_to_string(bytes);
    if (encoding == "utf-8") {
        if (const auto error = validate_utf8(text)) {
            return Result<std::string>::failure(error.code, error.message, error.context);
        }
        return Result<std::string>::success(std::move(text));
    }
    std::string decoded;
    for (std::size_t i = 0; i < text.size(); ++i) {
        const auto ch = static_cast<unsigned char>(text[i]);
        if (ch < 0x80) {
            decoded.push_back(static_cast<char>(ch));
        } else if (encoding == "ascii") {
            return Result<std::string>::failure(ErrorCode::parse_error,
                "Non-ASCII byte in ASCII input", "ascii byte " + std::to_string(i));
        } else {
            decoded.push_back(static_cast<char>(0xc0 | (ch >> 6)));
            decoded.push_back(static_cast<char>(0x80 | (ch & 0x3f)));
        }
    }
    return Result<std::string>::success(std::move(decoded));
}
} // namespace doxtk
