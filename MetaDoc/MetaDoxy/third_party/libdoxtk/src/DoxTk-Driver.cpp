#include "doxtk/DoxTk-Driver.hpp"
#include "doxtk/DoxTk-Parser.hpp"
#include "doxtk/DoxTk-Serializer.hpp"
#include "doxtk/DoxTk-Tooling.hpp"
#include "doxtk/DoxTk-Translator.hpp"

namespace doxtk {
namespace {
Error validate_options(const ConvertOptions& options) {
    if (options.source_format.empty() || options.target_format.empty()) {
        return {ErrorCode::invalid_argument, "Specify both source_format and target_format", "convert"};
    }
    const auto* source = find_format(options.source_format);
    const auto* target = find_format(options.target_format);
    if (!source || !source->readable) {
        return {ErrorCode::unsupported_format, "No reader available; supported input: plaintext, markdown", options.source_format};
    }
    if (!target || !target->writable) {
        return {ErrorCode::unsupported_format,
            "No writer available; supported output: plaintext, json-dump, xml-dump, ir-json, ir-xml, html", options.target_format};
    }
    return {};
}
}

Result<std::vector<std::byte>> convert(std::span<const std::byte> input, const ConvertOptions& options) {
    using Output = Result<std::vector<std::byte>>;
    if (const auto error = validate_options(options)) {
        return Output::failure(error.code, error.message, error.context);
    }
    auto document = parse_document(input, options.source_format, options.source_encoding);
    if (!document) return Output::failure(document.error.code, document.error.message, document.error.context);
    const auto registry = builtin_conversions();
    auto translated = registry.translate(*document.value, find_format(options.target_format)->ir, options);
    if (!translated) return Output::failure(translated.error.code, translated.error.message, translated.error.context);
    auto output = serialize_document(*translated.value, options.target_format);
    output.diagnostics = std::move(translated.diagnostics);
    return output;
}

Error convert_file(const std::filesystem::path& input, const std::filesystem::path& output,
                   const ConvertOptions& options) {
    return convert_file_with_diagnostics(input, output, options).error;
}

Result<std::filesystem::path> convert_file_with_diagnostics(const std::filesystem::path& input,
    const std::filesystem::path& output, const ConvertOptions& options) {
    using Output = Result<std::filesystem::path>;
    if (const auto error = validate_options(options)) return Output::failure(error.code, error.message, error.context);
    auto bytes = read_file(input);
    if (!bytes) return Output::failure(bytes.error.code, bytes.error.message, bytes.error.context);
    auto converted = convert(*bytes.value, options);
    if (!converted) {
        auto error = converted.error;
        error.context = input.string() + ": " + error.context;
        return Output::failure(error.code, error.message, error.context);
    }
    if (const auto error = write_file(output, *converted.value)) {
        auto failure = Output::failure(error.code, error.message, error.context);
        failure.diagnostics = std::move(converted.diagnostics);
        return failure;
    }
    return Output::success(output, std::move(converted.diagnostics));
}
} // namespace doxtk
