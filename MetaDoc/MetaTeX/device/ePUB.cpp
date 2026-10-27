#include "MetaTeX-Device.h"
#include "TextLayout.hpp"
#include "Vector.h"
#include "../utils/Format.hpp"
#include "../utils/Hash.hpp"
#include <limits>
namespace metatex {
namespace {
struct Entry { std::string name, bytes; std::uint32_t offset = 0, crc = 0; };
void u16(std::string& out, std::uint16_t n) { out += static_cast<char>(n); out += static_cast<char>(n >> 8); }
void u32(std::string& out, std::uint32_t n) { u16(out, n & 65535); u16(out, n >> 16); }
std::uint32_t size32(std::size_t size) {
    if (size > std::numeric_limits<std::uint32_t>::max()) throw std::length_error("EPUB exceeds ZIP32 limit");
    return static_cast<std::uint32_t>(size);
}
std::string zip(std::vector<Entry> entries) {
    std::string out;
    for (auto& entry : entries) {
        entry.offset = size32(out.size()); entry.crc = util::crc32(entry.bytes);
        auto size = size32(entry.bytes.size());
        u32(out, 0x04034b50); u16(out, 20); u16(out, 0); u16(out, 0); u16(out, 0); u16(out, 33);
        u32(out, entry.crc); u32(out, size); u32(out, size); u16(out, static_cast<std::uint16_t>(entry.name.size())); u16(out, 0);
        out += entry.name; out += entry.bytes;
    }
    auto start = size32(out.size());
    for (const auto& entry : entries) {
        auto size = size32(entry.bytes.size());
        u32(out, 0x02014b50); u16(out, 20); u16(out, 20); u16(out, 0); u16(out, 0); u16(out, 0); u16(out, 33);
        u32(out, entry.crc); u32(out, size); u32(out, size); u16(out, static_cast<std::uint16_t>(entry.name.size()));
        u16(out, 0); u16(out, 0); u16(out, 0); u16(out, 0); u32(out, 0); u32(out, entry.offset); out += entry.name;
    }
    auto centralSize = size32(out.size()) - start;
    u32(out, 0x06054b50); u16(out, 0); u16(out, 0); u16(out, static_cast<std::uint16_t>(entries.size()));
    u16(out, static_cast<std::uint16_t>(entries.size())); u32(out, centralSize); u32(out, start); u16(out, 0);
    return out;
}
std::string xml(std::string_view text) { util::validateUTF8(text); return util::xmlEscape(text); }
std::string inlineContent(const Document& doc, const Block& block, bool& math) {
    std::string result;
    for (const auto& s : block.spans) {
        auto text = xml(s.text);
        switch (s.kind) {
        case SpanKind::Bold: result += "<strong>" + text + "</strong>"; break;
        case SpanKind::Italic: result += "<em>" + text + "</em>"; break;
        case SpanKind::Code: result += "<code>" + text + "</code>"; break;
        case SpanKind::Underline: result += "<span style=\"text-decoration:underline\">" + text + "</span>"; break;
        case SpanKind::SmallCaps: result += "<span style=\"font-variant:small-caps\">" + text + "</span>"; break;
        case SpanKind::Math: result += renderMathML(s.text); math = true; break;
        case SpanKind::Link:
            if (s.extra.starts_with("ref:")) {
                auto found = doc.references.find(s.extra.substr(4)); result += found == doc.references.end() ? "??" : xml(found->second);
            } else if (s.extra.starts_with("https://") || s.extra.starts_with("http://"))
                result += "<a href=\"" + xml(s.extra) + "\">" + text + "</a>";
            else result += text;
            break;
        // The semantic Image span is a reference, not an embedded image asset.
        case SpanKind::Image: result += "<span>[image: " + text + " (" + xml(s.extra) + ")]</span>"; break;
        case SpanKind::Text: result += text; break;
        }
    }
    return result;
}
}
std::string renderEPUB(const Document& doc) {
    auto title = xml(doc.title.empty() ? "MetaTeX document" : doc.title);
    const std::string start = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<html xmlns=\"http://www.w3.org/1999/xhtml\" xmlns:epub=\"http://www.idpf.org/2007/ops\" xml:lang=\"en\"><head><title>" + title + "</title></head><body>";
    std::string content = start, navigation = start + "<nav epub:type=\"toc\" id=\"toc\"><h1>Contents</h1><ol>";
    bool hasMath = false, hasSVG = false, hasHeading = false; std::size_t number = 0;
    for (const auto& block : doc.blocks) {
        const auto id = "b" + std::to_string(++number); auto text = inlineContent(doc, block, hasMath);
        switch (block.kind) {
        case BlockKind::Heading: {
            auto level = std::to_string(std::clamp(block.level + 1, 1, 6));
            content += "<h" + level + " id=\"" + id + "\">" + text + "</h" + level + ">";
            navigation += "<li><a href=\"content.xhtml#" + id + "\">" + xml(detail::blockText(doc, block)) + "</a></li>"; hasHeading = true; break;
        }
        case BlockKind::Code: content += "<pre id=\"" + id + "\"><code>" + xml(detail::blockText(doc, block)) + "</code></pre>"; break;
        case BlockKind::Quote: content += "<blockquote id=\"" + id + "\"><p>" + text + "</p></blockquote>"; break;
        case BlockKind::Note: content += "<aside id=\"" + id + "\"><p>" + text + "</p></aside>"; break;
        case BlockKind::HorizontalRule: content += "<hr id=\"" + id + "\"/>"; break;
        case BlockKind::PageBreak: content += "<div id=\"" + id + "\" style=\"break-before:page\"></div>"; break;
        case BlockKind::TableOfContents: content += "<p><a href=\"nav.xhtml\">Contents</a></p>"; break;
        case BlockKind::VectorGraphic: {
            auto scene = detail::sceneOf(block); auto svg = detail::vectorStream(); hasSVG = true;
            svg << "<figure id=\"" << id << "\"><svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 " << scene.width << ' ' << scene.height
                << "\"><title>" << xml(detail::blockText(doc, block)) << "</title>" << detail::svgPaths(scene) << "</svg><figcaption>" << text << "</figcaption></figure>";
            content += svg.str(); break;
        }
        case BlockKind::ListItem: content += "<p id=\"" + id + "\">" + (block.ordered ? std::to_string(std::max(1,block.listNumber))+". " : "• ") + text + "</p>"; break;
        case BlockKind::Paragraph: case BlockKind::DisplayMath: content += "<p id=\"" + id + "\">" + text + "</p>"; break;
        }
    }
    if (!hasHeading) navigation += "<li><a href=\"content.xhtml\">" + title + "</a></li>";
    content += "</body></html>"; navigation += "</ol></nav></body></html>";
    std::string properties = hasMath ? "mathml" : "";
    if (hasSVG) properties += properties.empty() ? "svg" : " svg";
    auto identifier = "urn:metatex:" + util::hex64(util::hash64(content));
    std::string package = "<?xml version=\"1.0\" encoding=\"UTF-8\"?><package xmlns=\"http://www.idpf.org/2007/opf\" version=\"3.0\" unique-identifier=\"book-id\"><metadata xmlns:dc=\"http://purl.org/dc/elements/1.1/\">"
        "<dc:identifier id=\"book-id\">" + identifier + "</dc:identifier><dc:title>" + title + "</dc:title><dc:language>en</dc:language>";
    if (!doc.author.empty()) package += "<dc:creator>" + xml(doc.author) + "</dc:creator>";
    // Stable timestamp makes byte-identical documents reproducible.
    package += "<meta property=\"dcterms:modified\">1970-01-01T00:00:00Z</meta></metadata><manifest>"
        "<item id=\"content\" href=\"content.xhtml\" media-type=\"application/xhtml+xml\"" +
        (properties.empty() ? "" : " properties=\"" + properties + "\"") + "/>"
        "<item id=\"nav\" href=\"nav.xhtml\" media-type=\"application/xhtml+xml\" properties=\"nav\"/>"
        "</manifest><spine><itemref idref=\"content\"/></spine></package>";
    return zip({{"mimetype", "application/epub+zip"},
        {"META-INF/container.xml", "<?xml version=\"1.0\"?><container version=\"1.0\" xmlns=\"urn:oasis:names:tc:opendocument:xmlns:container\"><rootfiles><rootfile full-path=\"OEBPS/package.opf\" media-type=\"application/oebps-package+xml\"/></rootfiles></container>"},
        {"OEBPS/package.opf", std::move(package)}, {"OEBPS/content.xhtml", std::move(content)}, {"OEBPS/nav.xhtml", std::move(navigation)}});
}
}
