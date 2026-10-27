#pragma once

#include "MetaTeX.h"
#include "MetaTeX-Device.h"
#include <DSLtk.hpp>
#include <cmath>
#include <concepts>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace metatex {

// All renderers consume the same semantic IR, including documents built in C++.
enum class OutputFormat { HTML, PDF, JSON, Text, PostScript, TroffIR, EPUB };

inline std::string render(const Document& doc, OutputFormat format) {
    switch (format) {
    case OutputFormat::HTML: return renderHTML(doc);
    case OutputFormat::PDF: return renderPDF(doc);
    case OutputFormat::JSON: return renderJSON(doc);
    case OutputFormat::Text: return renderText(doc);
    case OutputFormat::PostScript: return renderPostScript(doc);
    case OutputFormat::TroffIR: return renderTroffIR(doc);
    case OutputFormat::EPUB: return renderEPUB(doc);
    }
    throw std::invalid_argument("unknown MetaTeX output format");
}

// PDF here is the in-memory libHaru device; renderCairoPDF remains available for
// callers who want the optional shaped, file-based PDF backend.
inline void writeDocument(const Document& doc, const std::filesystem::path& path,
                          OutputFormat format) {
    writeFile(path, render(doc, format));
}

inline bool hasErrors(const Document& doc) {
    for (const auto& diagnostic : doc.diagnostics)
        if (diagnostic.severity == Diagnostic::Severity::Error) return true;
    return false;
}

inline Document compile(std::string_view source,
                        const std::filesystem::path& filename = "<memory>") {
    Engine engine;
    return engine.compile(std::string(source), filename);
}

inline Document compileFile(Engine& engine, const std::filesystem::path& path) {
    return engine.compile(readFile(path), path);
}

inline Document compileFile(const std::filesystem::path& path) {
    Engine engine;
    return compileFile(engine, path);
}

// Owns its document. build() snapshots lvalues and transfers rvalues, so neither
// builders nor DSL expressions retain views into caller-owned strings.
class DocumentBuilder : public ::dsl::DSL<DocumentBuilder, ::dsl::Pipeline> {
public:
    DocumentBuilder() = default;
    explicit DocumentBuilder(Document doc) : document_(std::move(doc)) {}

    DocumentBuilder& title(std::string value) {
        document_.title = std::move(value); return *this;
    }
    DocumentBuilder& author(std::string value) {
        document_.author = std::move(value); return *this;
    }
    DocumentBuilder& date(std::string value) {
        document_.date = std::move(value); return *this;
    }
    DocumentBuilder& append(Block block) {
        document_.blocks.push_back(std::move(block)); return *this;
    }
    DocumentBuilder& reference(std::string key, std::string value) {
        document_.references.insert_or_assign(std::move(key), std::move(value));
        return *this;
    }
    DocumentBuilder& diagnostic(Diagnostic value) {
        document_.diagnostics.push_back(std::move(value)); return *this;
    }
    Document build() const & { return document_; }
    Document build() && { return std::move(document_); }

private:
    Document document_;
};

// Native C++ vocabulary. Factories create ordinary Span/Block/Document values;
// pipeline stages are DSLtk PipeStages and can mix with user-defined dsl::pipe.
namespace cpp {

inline Span text(std::string_view value) { return {SpanKind::Text, std::string(value), {}}; }
inline Span bold(std::string_view value) { return {SpanKind::Bold, std::string(value), {}}; }
inline Span italic(std::string_view value) { return {SpanKind::Italic, std::string(value), {}}; }
inline Span code(std::string_view value) { return {SpanKind::Code, std::string(value), {}}; }
inline Span smallCaps(std::string_view value) { return {SpanKind::SmallCaps, std::string(value), {}}; }
inline Span underline(std::string_view value) { return {SpanKind::Underline, std::string(value), {}}; }
// Math uses the existing TeX-math subset, not text expansion or command execution.
inline Span math(std::string_view value) { return {SpanKind::Math, std::string(value), {}}; }
inline Span link(std::string_view label, std::string_view target) {
    return {SpanKind::Link, std::string(label), std::string(target)};
}
inline Span image(std::string_view alt, std::string_view source) {
    return {SpanKind::Image, std::string(alt), std::string(source)};
}
inline Span ref(std::string_view key) { return link({}, "ref:" + std::string(key)); }
// std::string brings std::ref into unqualified lookup through ADL. Exact,
// non-template overloads keep the DSL spelling usable with dynamic keys too.
inline Span ref(std::string& key) { return cpp::ref(std::string_view(key)); }
inline Span ref(const std::string& key) { return cpp::ref(std::string_view(key)); }
inline Span ref(std::string&& key) { return cpp::ref(std::string_view(key)); }
inline Span ref(const std::string&& key) { return cpp::ref(std::string_view(key)); }
inline Span ref(const char* key) { return cpp::ref(std::string_view(key)); }

namespace detail {
inline Span asSpan(Span value) { return value; }
inline Span asSpan(std::string_view value) { return text(value); }
template<class T>
concept InlineContent = std::same_as<std::remove_cvref_t<T>, Span> ||
                        std::convertible_to<T, std::string_view>;
}

template<detail::InlineContent... Content>
Block block(BlockKind kind, Content&&... content) {
    Block result;
    result.kind = kind;
    result.spans.reserve(sizeof...(Content));
    (result.spans.push_back(detail::asSpan(std::forward<Content>(content))), ...);
    return result;
}

template<detail::InlineContent... Content>
Block paragraph(Content&&... content) {
    return block(BlockKind::Paragraph, std::forward<Content>(content)...);
}
// IR levels: 0 = document title, 1 = section, ... 5 = deepest HTML heading.
// Headings are unnumbered; IDs and reference display values are explicit.
template<detail::InlineContent... Content>
Block heading(int level, Content&&... content) {
    if (level < 0 || level > 5) throw std::invalid_argument("heading level must be 0..5");
    auto result = block(BlockKind::Heading, std::forward<Content>(content)...);
    result.level = level;
    return result;
}
template<detail::InlineContent... Content>
Block listItem(int level, bool ordered, Content&&... content) {
    if (level < 1) throw std::invalid_argument("list level must be positive");
    auto result = block(BlockKind::ListItem, std::forward<Content>(content)...);
    result.level = level;
    result.ordered = ordered;
    return result;
}
template<detail::InlineContent... Content>
Block quote(Content&&... content) {
    return block(BlockKind::Quote, std::forward<Content>(content)...);
}
template<detail::InlineContent... Content>
Block note(Content&&... content) {
    return block(BlockKind::Note, std::forward<Content>(content)...);
}
inline Block codeBlock(std::string_view value) { return block(BlockKind::Code, code(value)); }
inline Block displayMath(std::string_view value) { return block(BlockKind::DisplayMath, math(value)); }
inline Block horizontalRule() { return block(BlockKind::HorizontalRule); }
inline Block tableOfContents() { return block(BlockKind::TableOfContents); }
inline Block pageBreak() { return block(BlockKind::PageBreak); }
inline Block graphic(metavect::Scene scene, std::string_view caption = {}, double width = 0) {
    if (!std::isfinite(width) || width < 0 || (width > 0 && width < 0.01) || width > 1000000)
        throw std::invalid_argument("graphic width must be 0 (natural) or 0.01..1000000 points");
    auto result = block(BlockKind::VectorGraphic, text(caption));
    result.graphic = std::make_shared<const metavect::Scene>(std::move(scene));
    result.graphicWidth = width;
    return result;
}

template<class... Blocks> requires (std::same_as<std::remove_cvref_t<Blocks>, Block> && ...)
Document document(Blocks&&... blocks) {
    Document result;
    result.blocks.reserve(sizeof...(Blocks));
    (result.blocks.push_back(std::forward<Blocks>(blocks)), ...);
    return result;
}

inline auto withId(std::string id) {
    return ::dsl::pipe([id = std::move(id)](Block value) {
        value.id = id; return value;
    });
}
inline auto title(std::string value) {
    return ::dsl::pipe([value = std::move(value)](Document doc) {
        doc.title = value; return doc;
    });
}
inline auto author(std::string value) {
    return ::dsl::pipe([value = std::move(value)](Document doc) {
        doc.author = value; return doc;
    });
}
inline auto date(std::string value) {
    return ::dsl::pipe([value = std::move(value)](Document doc) {
        doc.date = value; return doc;
    });
}
inline auto reference(std::string key, std::string value) {
    return ::dsl::pipe([key = std::move(key), value = std::move(value)](Document doc) {
        doc.references.insert_or_assign(key, value); return doc;
    });
}
inline auto append(Block value) {
    return ::dsl::pipe([value = std::move(value)](Document doc) {
        doc.blocks.push_back(value); return doc;
    });
}
// The engine is borrowed and must outlive this reusable stage. Its registered
// commands/plugins participate exactly as in Engine::compile.
inline auto compileWith(Engine& engine, std::filesystem::path filename = "<memory>") {
    return ::dsl::pipe([&engine, filename = std::move(filename)](std::string_view source) {
        return engine.compile(std::string(source), filename);
    });
}
inline auto renderAs(OutputFormat format) {
    return ::dsl::pipe([format](const Document& doc) { return render(doc, format); });
}
inline auto writeTo(std::filesystem::path path, OutputFormat format) {
    return ::dsl::pipe([path = std::move(path), format](Document doc) {
        writeDocument(doc, path, format); return doc;
    });
}

namespace literals {
inline Span operator""_text(const char* value, std::size_t size) {
    return text(std::string_view(value, size));
}
inline Span operator""_math(const char* value, std::size_t size) {
    return math(std::string_view(value, size));
}
} // namespace literals
} // namespace cpp
} // namespace metatex
