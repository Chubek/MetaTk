#include "doxtk/DoxTk-Parser.hpp"
#include "doxtk/DoxTk-Bytestream.hpp"
#include "doxtk/DoxTk-Encoding.hpp"
#include "doxtk/DoxTk-Markdown.hpp"

namespace doxtk {
Result<Document> parse_document(std::span<const std::byte> input,
                               std::string_view format, std::string_view encoding) {
    if (format == "markdown") {
        return parse_markdown(input, encoding);
    }
    if (format != "plaintext") {
        return Result<Document>::failure(ErrorCode::unsupported_format,
            "No reader for this format; supported input: plaintext, markdown", std::string(format));
    }
    auto text = decode_text(input, encoding);
    if (!text) return Result<Document>::failure(text.error.code, text.error.message, text.error.context);
    Document document;
    document.ir = "Plaintext";
    document.media_type = "text/plain";
    document.data = to_bytes(*text.value);
    return Result<Document>::success(std::move(document));
}
} // namespace doxtk
