#pragma once
#include <cstdint>
#include <iomanip>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
namespace metatex::util {
inline std::string xmlEscape(std::string_view text) {
    std::string result;
    for (unsigned char c : text) {
        switch (c) {
        case '&': result += "&amp;"; break;
        case '<': result += "&lt;"; break;
        case '>': result += "&gt;"; break;
        case '\"': result += "&quot;"; break;
        case '\'': result += "&apos;"; break;
        default:
            if (c < 32 && c != '\n' && c != '\r' && c != '\t')
                throw std::invalid_argument("XML text contains a control character");
            result += static_cast<char>(c);
        }
    }
    return result;
}
inline std::string hex64(std::uint64_t value) {
    std::ostringstream out; out.imbue(std::locale::classic());
    out << std::hex << std::setfill('0') << std::setw(16) << value;
    return out.str();
}
inline std::string diagnosticText(std::string_view text) {
    std::string result;
    for (unsigned char c : text) {
        if (c < 32 || c == 127) {
            constexpr char hex[] = "0123456789abcdef";
            result += "\\x"; result += hex[c >> 4]; result += hex[c & 15];
        } else result += static_cast<char>(c);
    }
    return result;
}
}
