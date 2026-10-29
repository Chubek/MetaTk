#include "doxtk/DoxTk-Serializer.hpp"
#include "doxtk/DoxTk-Bytestream.hpp"
#include "doxtk/DoxTk-Encoding.hpp"
#include "doxtk/DoxTk-Tooling.hpp"
#include "doxtk/DoxTk-HTML.hpp"
#include <cstdint>
#include <map>

namespace doxtk {
namespace {
std::string json_string(std::string_view text) {
    constexpr char hex[] = "0123456789abcdef";
    std::string output = "\"";
    for (unsigned char ch : text) {
        switch (ch) {
        case '"': output += "\\\""; break;
        case '\\': output += "\\\\"; break;
        case '\b': output += "\\b"; break;
        case '\f': output += "\\f"; break;
        case '\n': output += "\\n"; break;
        case '\r': output += "\\r"; break;
        case '\t': output += "\\t"; break;
        default:
            if (ch < 0x20) {
                output += "\\u00";
                output.push_back(hex[ch >> 4]);
                output.push_back(hex[ch & 0xf]);
            } else {
                output.push_back(static_cast<char>(ch));
            }
        }
    }
    return output + '"';
}

// Called only after UTF-8 validation. XML 1.0 cannot represent all Unicode
// scalars (notably NUL), even as character references.
Error validate_xml_text(std::string_view text) {
    for (std::size_t i = 0; i < text.size();) {
        const auto start = i;
        const auto lead = static_cast<unsigned char>(text[i++]);
        std::uint32_t scalar = lead;
        std::size_t count = 0;
        if (lead >= 0xf0) { scalar = lead & 0x07; count = 3; }
        else if (lead >= 0xe0) { scalar = lead & 0x0f; count = 2; }
        else if (lead >= 0xc0) { scalar = lead & 0x1f; count = 1; }
        for (std::size_t n = 0; n < count; ++n) {
            scalar = (scalar << 6) | (static_cast<unsigned char>(text[i++]) & 0x3f);
        }
        if (!((scalar == 9 || scalar == 10 || scalar == 13) ||
              (scalar >= 0x20 && scalar <= 0xd7ff) ||
              (scalar >= 0xe000 && scalar <= 0xfffd) ||
              (scalar >= 0x10000 && scalar <= 0x10ffff))) {
            return {ErrorCode::serialization_error, "Character cannot be represented in XML 1.0; use json-dump",
                "xml-dump byte " + std::to_string(start)};
        }
    }
    return {};
}

std::string xml_text(std::string_view text) {
    std::string output;
    for (char ch : text) {
        switch (ch) {
        case '&': output += "&amp;"; break;
        case '<': output += "&lt;"; break;
        case '>': output += "&gt;"; break;
        case '"': output += "&quot;"; break;
        case '\'': output += "&apos;"; break;
        case '\r': output += "&#13;"; break;
        case '\n': output += "&#10;"; break;
        case '\t': output += "&#9;"; break;
        default: output.push_back(ch);
        }
    }
    return output;
}
}

Result<std::vector<std::byte>> serialize_document(const Document& document, std::string_view format) {
    using Output = Result<std::vector<std::byte>>;
    const auto* info = find_format(format);
    if (!info || !info->writable) {
        return Output::failure(ErrorCode::unsupported_format,
            "No writer for this format; supported output: plaintext, json-dump, xml-dump", std::string(format));
    }
    if (document.ir != info->ir) {
        return Output::failure(ErrorCode::unsupported_conversion,
            "Writer requires IR " + std::string(info->ir), document.ir + " -> " + std::string(format));
    }
    auto text = decode_text(document.data, document.encoding);
    if (!text) return Output::failure(text.error.code, text.error.message, text.error.context);
    if (format == "ir-json" || format == "ir-xml") {
        // These IRs contain already-serialized, UTF-8 inspection envelopes.
        return Output::success(to_bytes(*text.value));
    }
    if (format == "html") {
        if (document.ir != "MDAST") {
            return Output::failure(ErrorCode::unsupported_conversion,
                "HTML output requires MDAST IR", document.ir + " -> html");
        }
        auto html = html::serialize_html(document);
        if (!html) return Output::failure(html.error.code, html.error.message, html.error.context);
        return Output::success(std::move(*html.value));
    }
    if (format == "plaintext") {
        if (!document.metadata.empty()) {
            return Output::failure(ErrorCode::unsupported_conversion,
                "Plaintext output cannot preserve metadata; use json-dump or xml-dump", "serialize plaintext");
        }
        return Output::success(to_bytes(*text.value));
    }
    const std::map<std::string, std::string> metadata(document.metadata.begin(), document.metadata.end());
    const auto validate = [format](std::string_view value) -> Error {
        if (const auto error = validate_utf8(value)) return error;
        if (format == "xml-dump") return validate_xml_text(value);
        return {};
    };
    for (const auto& [key, value] : metadata) {
        if (auto error = validate(key)) {
            error.context = "metadata key: " + error.context;
            return Output::failure(error.code, error.message, error.context);
        }
        if (auto error = validate(value)) {
            error.context = "metadata value: " + error.context;
            return Output::failure(error.code, error.message, error.context);
        }
    }
    if (const auto error = validate(document.media_type)) {
        return Output::failure(error.code, error.message, "media_type: " + error.context);
    }
    std::string output;
    if (format == "json-dump") {
        output = "{\"ir\":\"Plaintext\",\"media_type\":" + json_string(document.media_type) +
            ",\"encoding\":\"utf-8\",\"text\":" + json_string(*text.value) + ",\"metadata\":{";
        bool first = true;
        for (const auto& [key, value] : metadata) {
            if (!first) output += ',';
            first = false;
            output += json_string(key) + ':' + json_string(value);
        }
        output += "}}\n";
    } else {
        if (const auto error = validate_xml_text(*text.value)) {
            return Output::failure(error.code, error.message, error.context);
        }
        output = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<document ir=\"Plaintext\" media_type=\"" +
            xml_text(document.media_type) + "\" encoding=\"utf-8\"><text>" + xml_text(*text.value) + "</text><metadata>";
        for (const auto& [key, value] : metadata) {
            output += "<entry key=\"" + xml_text(key) + "\">" + xml_text(value) + "</entry>";
        }
        output += "</metadata></document>\n";
    }
    return Output::success(to_bytes(output));
}
} // namespace doxtk
