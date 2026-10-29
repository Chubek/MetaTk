#include "doxtk/DoxTk-IRDump.hpp"
#include "doxtk/DoxTk-Bytestream.hpp"
#include "doxtk/DoxTk-Encoding.hpp"
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

std::string json_position(const SourcePosition& pos) {
    if (!pos.valid) return "null";
    return "{\"line\":" + std::to_string(pos.line) + ",\"column\":" + std::to_string(pos.column)
        + ",\"offset\":" + std::to_string(pos.offset) + "}";
}

std::string json_node(const Node& node) {
    std::string out = "{\"kind\":" + json_string(node.kind);
    if (!node.text.empty()) out += ",\"text\":" + json_string(node.text);
    if (!node.attributes.empty()) {
        out += ",\"attributes\":[";
        bool first = true;
        for (const auto& [key, value] : node.attributes) {
            if (!first) out += ',';
            first = false;
            out += "{\"key\":" + json_string(key) + ",\"value\":" + json_string(value) + "}";
        }
        out += "]";
    } else {
        out += ",\"attributes\":[]";
    }
    out += ",\"start\":" + json_position(node.start);
    out += ",\"end\":" + json_position(node.end);
    out += ",\"children\":[";
    bool first = true;
    for (const auto& child : node.children) {
        if (!first) out += ',';
        first = false;
        out += json_node(child);
    }
    out += "]}";
    return out;
}

std::string_view kind_to_tag(std::string_view kind) {
    if (kind == "document") return "document";
    if (kind == "paragraph") return "paragraph";
    if (kind == "heading") return "heading";
    if (kind == "blockquote") return "blockquote";
    if (kind == "list") return "list";
    if (kind == "listItem") return "listItem";
    if (kind == "code") return "code";
    if (kind == "thematicBreak") return "thematicBreak";
    if (kind == "html") return "html";
    if (kind == "definition") return "definition";
    if (kind == "text") return "text";
    if (kind == "emphasis") return "emphasis";
    if (kind == "strong") return "strong";
    if (kind == "inlineCode") return "inlineCode";
    if (kind == "link") return "link";
    if (kind == "image") return "image";
    if (kind == "break") return "break";
    if (kind == "verbatim") return "verbatim";
    return "unknown";
}

std::string xml_escape(std::string_view text) {
    std::string out;
    for (unsigned char ch : text) {
        if (ch == '&') out += "&";
        else if (ch == '<') out += "<";
        else if (ch == '>') out += ">";
        else if (ch == '"') out += "\"";
        else if (ch == '\'') out += "'";
        else if (ch == '\r') out += "&#13;";
        else if (ch == '\n') out += "&#10;";
        else if (ch == '\t') out += "&#9;";
        else out.push_back(static_cast<char>(ch));
    }
    return out;
}

Error validate_xml_scalar_text(std::string_view text) {
    for (std::size_t i = 0; i < text.size();) {
        const auto start = i;
        const auto lead = static_cast<unsigned char>(text[i++]);
        std::uint32_t scalar = lead, count = 0;
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
            return {ErrorCode::serialization_error, "Character cannot be represented in XML 1.0; use ir-json",
                "ir-xml byte " + std::to_string(start)};
        }
    }
    return {};
}

Error validate_xml_node(const Node& node) {
    if (const auto error = validate_xml_scalar_text(node.text)) return error;
    for (const auto& [key, value] : node.attributes) {
        if (const auto error = validate_xml_scalar_text(key)) return error;
        if (const auto error = validate_xml_scalar_text(value)) return error;
    }
    for (const auto& child : node.children) {
        if (const auto error = validate_xml_node(child)) return error;
    }
    return {};
}

std::string xml_node(const Node& node) {
    const std::string_view tag = kind_to_tag(node.kind);
    std::string out = std::string("<") + std::string(tag);
    if (!node.kind.empty()) out += " kind=\"" + xml_escape(node.kind) + "\"";
    if (!node.text.empty()) out += " text=\"" + xml_escape(node.text) + "\"";
    for (const auto& [key, value] : node.attributes) {
        out += " " + std::string(key) + "=\"" + xml_escape(value) + "\"";
    }
    if (!node.children.empty()) {
        out += ">";
        for (const auto& child : node.children) {
            out += xml_node(child);
        }
        out += "</" + std::string(tag) + ">";
    } else {
        out += " />";
    }
    return out;
}
} // namespace

Result<std::string> to_ir_json(const Node& root, std::string_view ir, std::string_view media_type,
                                 const std::unordered_map<std::string, std::string>& metadata) {
    using Output = Result<std::string>;
    if (ir.empty()) return Output::failure(ErrorCode::invalid_argument, "IR name is required", "to_ir_json");
    if (const auto error = validate_utf8(ir)) return Output::failure(error.code, error.message, "ir: " + error.context);
    auto err = root.validate();
    if (err) return Output::failure(err.code, err.message, err.context);
    if (metadata.size() > 0) {
        for (const auto& [k, v] : metadata) {
            if (const auto e = validate_utf8(k)) return Output::failure(e.code, e.message, "metadata key: " + e.context);
            if (const auto e = validate_utf8(v)) return Output::failure(e.code, e.message, "metadata value: " + e.context);
        }
    }
    std::string out = "{\"ir\":" + json_string(ir) + ",\"media_type\":" + json_string(media_type)
        + ",\"json_type\":\"ast-node\",\"metadata\":{";
    std::map<std::string, std::string> sorted(metadata.begin(), metadata.end());
    bool first = true;
    for (const auto& [key, value] : sorted) {
        if (!first) out += ',';
        first = false;
        out += json_string(key) + ':' + json_string(value);
    }
    out += "},\"root\":" + json_node(root) + "}\n";
    return Output::success(std::move(out));
}

Result<std::string> to_ir_xml(const Node& root, std::string_view ir, std::string_view media_type,
                                const std::unordered_map<std::string, std::string>& metadata) {
    using Output = Result<std::string>;
    if (ir.empty()) return Output::failure(ErrorCode::invalid_argument, "IR name is required", "to_ir_xml");
    auto err = root.validate();
    if (err) return Output::failure(err.code, err.message, err.context);
    if (const auto error = validate_utf8(ir)) return Output::failure(error.code, error.message, "ir: " + error.context);
    if (!media_type.empty()) {
        if (const auto e = validate_utf8(media_type)) return Output::failure(e.code, e.message, "media_type: " + e.context);
        if (const auto e = validate_xml_scalar_text(media_type)) return Output::failure(e.code, e.message, "media_type: " + e.context);
    }
    for (const auto& [k, v] : metadata) {
        if (const auto e = validate_utf8(k)) return Output::failure(e.code, e.message, "metadata key: " + e.context);
        if (const auto e = validate_utf8(v)) return Output::failure(e.code, e.message, "metadata value: " + e.context);
        if (const auto e = validate_xml_scalar_text(k)) return Output::failure(e.code, e.message, "metadata key: " + e.context);
        if (const auto e = validate_xml_scalar_text(v)) return Output::failure(e.code, e.message, "metadata value: " + e.context);
    }
    if (const auto error = validate_xml_node(root)) return Output::failure(error.code, error.message, error.context);
    std::string out = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<ir kind=\"ast\" ir=\"" + xml_escape(ir) +
        "\" media_type=\"" + xml_escape(media_type) + "\"><metadata>";
    std::map<std::string, std::string> sorted(metadata.begin(), metadata.end());
    for (const auto& [key, value] : sorted) {
        out += "<entry key=\"" + xml_escape(key) + "\">" + xml_escape(value) + "</entry>";
    }
    out += "</metadata>" + xml_node(root) + "</ir>\n";
    return Output::success(std::move(out));
}
} // namespace doxtk
