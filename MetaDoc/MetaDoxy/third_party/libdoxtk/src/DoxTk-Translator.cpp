#include "doxtk/DoxTk-Translator.hpp"
#include "doxtk/DoxTk-Encoding.hpp"
#include "doxtk/DoxTk-Bytestream.hpp"
#include "doxtk/DoxTk-MDAST.hpp"
#include "doxtk/DoxTk-IRDump.hpp"
#include "doxtk/DoxTk-Markdown.hpp"

namespace doxtk {
namespace {
Result<std::shared_ptr<const Node>> mdast_tree(const Document& input) {
    using Tree = Result<std::shared_ptr<const Node>>;
    if (input.tree) {
        if (input.tree->kind != "document") return Tree::failure(ErrorCode::invalid_argument, "MDAST root must be a document", "translate MDAST");
        if (const auto error = input.tree->validate()) return Tree::failure(error.code, error.message, error.context);
        return Tree::success(input.tree);
    }
    auto source = decode_text(input.data, input.encoding);
    if (!source) return Tree::failure(source.error.code, source.error.message, source.error.context);
    auto node = mdast::parse(*source.value);
    if (!node) return Tree::failure(node.error.code, node.error.message, node.error.context);
    return Tree::success(std::make_shared<const Node>(std::move(*node.value)));
}
}
Error ConversionRegistry::add(std::string source_ir, std::string target_ir, Translation translate) {
    if (source_ir.empty() || target_ir.empty() || !translate) {
        return {ErrorCode::invalid_argument, "A route requires two IR names and a callback", "register conversion"};
    }
    if (!routes_.emplace(std::make_pair(std::move(source_ir), std::move(target_ir)), std::move(translate)).second) {
        return {ErrorCode::invalid_argument, "Conversion route already registered", "register conversion"};
    }
    return {};
}

bool ConversionRegistry::supports(std::string_view source_ir, std::string_view target_ir) const {
    return routes_.contains({std::string(source_ir), std::string(target_ir)});
}

Result<Document> ConversionRegistry::translate(const Document& input, std::string_view target_ir,
                                              const ConvertOptions& options) const {
    if (input.ir.empty() || target_ir.empty()) {
        return Result<Document>::failure(ErrorCode::invalid_argument,
            "Source and target IR names must be explicit", "translate");
    }
    const auto route = routes_.find({input.ir, std::string(target_ir)});
    if (route == routes_.end()) {
        return Result<Document>::failure(ErrorCode::unsupported_conversion,
            "No direct conversion route registered", input.ir + " -> " + std::string(target_ir));
    }
    auto output = route->second(input, options);
    if (!output.value && !output.error) {
        return Result<Document>::failure(ErrorCode::internal_error, "Conversion returned neither a document nor an error", "translate");
    }
    if (output && output.value->ir != target_ir) {
        return Result<Document>::failure(ErrorCode::internal_error,
            "Conversion callback returned the wrong IR", input.ir + " -> " + std::string(target_ir));
    }
    return output;
}

ConversionRegistry builtin_conversions() {
    ConversionRegistry registry;
    const auto error = registry.add("Plaintext", "Plaintext", [](const Document& input, const ConvertOptions&) {
        auto text = decode_text(input.data, input.encoding);
        if (!text) return Result<Document>::failure(text.error.code, text.error.message, text.error.context);
        auto output = input;
        output.encoding = "utf-8";
        output.data = to_bytes(text.value.value());
        return Result<Document>::success(std::move(output));
    });
    const auto plaintext_error = registry.add("MDAST", "Plaintext", [](const Document& input, const ConvertOptions& options) {
        if (!options.allow_lossy) return Result<Document>::failure(ErrorCode::unsupported_conversion,
            "MDAST -> Plaintext loses structure; set allow_lossy or use --allow-lossy, or choose ir-json/ir-xml", "MDAST -> Plaintext");
        auto node = mdast_tree(input);
        if (!node) return Result<Document>::failure(node.error.code, node.error.message, node.error.context);
        auto text = mdast::to_plaintext(**node.value);
        if (!text) return Result<Document>::failure(text.error.code, text.error.message, text.error.context);
        auto output = input;
        output.ir = "Plaintext";
        output.media_type = "text/plain";
        output.encoding = "utf-8";
        output.data = to_bytes(text.value.value());
        output.tree.reset();
        return Result<Document>::success(std::move(output), std::move(text.diagnostics));
    });
    const auto json_error = registry.add("MDAST", "DoxTkJSON", [](const Document& input, const ConvertOptions&) {
        auto node = mdast_tree(input);
        if (!node) return Result<Document>::failure(node.error.code, node.error.message, node.error.context);
        auto json = to_ir_json(**node.value, "MDAST", input.media_type, input.metadata);
        if (!json) return Result<Document>::failure(json.error.code, json.error.message, json.error.context);
        auto output = input;
        output.ir = "DoxTkJSON";
        output.media_type = "application/json";
        output.encoding = "utf-8";
        output.tree.reset();
        output.data = to_bytes(json.value.value());
        return Result<Document>::success(std::move(output));
    });
    const auto xml_error = registry.add("MDAST", "DoxTkXML", [](const Document& input, const ConvertOptions&) {
        auto node = mdast_tree(input);
        if (!node) return Result<Document>::failure(node.error.code, node.error.message, node.error.context);
        auto xml = to_ir_xml(**node.value, "MDAST", input.media_type, input.metadata);
        if (!xml) return Result<Document>::failure(xml.error.code, xml.error.message, xml.error.context);
        auto output = input;
        output.ir = "DoxTkXML";
        output.media_type = "application/xml";
        output.encoding = "utf-8";
        output.tree.reset();
        output.data = to_bytes(xml.value.value());
        return Result<Document>::success(std::move(output));
    });
    (void)error; // A fresh registry cannot contain this route.
    (void)plaintext_error;
    (void)json_error;
    (void)xml_error;
    return registry;
}
} // namespace doxtk
