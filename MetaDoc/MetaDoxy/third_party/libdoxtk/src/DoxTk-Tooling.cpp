#include "doxtk/DoxTk-Tooling.hpp"
#include <array>

namespace doxtk {
namespace {
constexpr std::array formats{
    FormatInfo{"plaintext", "Plaintext", "text/plain", true, true},
    FormatInfo{"json-dump", "Plaintext", "application/json", false, true},
    FormatInfo{"xml-dump", "Plaintext", "application/xml", false, true},
    FormatInfo{"markdown", "MDAST", "text/markdown", true, false},
    FormatInfo{"ir-json", "DoxTkJSON", "application/json", false, true},
    FormatInfo{"ir-xml", "DoxTkXML", "application/xml", false, true},
    FormatInfo{"html", "MDAST", "text/html", false, true}
};
}

std::span<const FormatInfo> builtin_formats() noexcept { return formats; }

const FormatInfo* find_format(std::string_view name) noexcept {
    for (const auto& format : formats) {
        if (format.name == name) return &format;
    }
    return nullptr;
}
} // namespace doxtk
