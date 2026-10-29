#include "DoxTk.hpp"
#include "doxtk/DoxTk-Bytestream.hpp"
#include "doxtk/DoxTk-CLI.hpp"
#include "test_support.hpp"
#include <array>

using namespace doxtk;

int main() {
    const std::string text = std::string("Hello\0\r\n", 8) + "\xc3\xa9\xf0\x9f\x98\x80";
    CHECK(bytes_to_string(to_bytes(text)) == text);
    CHECK(!validate_utf8(text));
    CHECK(!validate_utf8("\xc2\x80\xdf\xbf\xe0\xa0\x80\xed\x9f\xbf\xef\xbf\xbf\xf0\x90\x80\x80\xf4\x8f\xbf\xbf"));
    for (std::string_view malformed : {"\x80", "\xc0\xaf", "\xc1\xbf", "\xc2", "\xc2x",
            "\xe0\x80\xaf", "\xed\xa0\x80", "\xf0\x80\x80\xaf", "\xf4\x90\x80\x80",
            "\xf5\x80\x80\x80", "\xff", "\xef\xbf", "\xf0\x90\x80"}) {
        const auto error = validate_utf8(malformed);
        CHECK(error.code == ErrorCode::parse_error);
        CHECK(!error.context.empty());
        CHECK(!parse_document(to_bytes(malformed), "plaintext"));
    }
    CHECK(*decode_text(to_bytes("\xe9\xff"), "iso-8859-1").value == "\xc3\xa9\xc3\xbf");
    CHECK(!decode_text(to_bytes("\x80"), "ascii"));
    CHECK(*decode_text(to_bytes("ascii"), "ascii").value == "ascii");
    CHECK(!decode_text({}, "utf-16"));
    CHECK(*decode_text({}, "utf-8").value == "");
    CHECK(*decode_text(to_bytes("\xef\xbb\xbf"), "utf-8").value == "\xef\xbb\xbf");

    auto parsed = parse_document(to_bytes(text), "plaintext");
    CHECK(parsed);
    auto document = *parsed.value;
    CHECK(document.ir == "Plaintext");
    CHECK(document.media_type == "text/plain");
    CHECK(document.encoding == "utf-8");
    CHECK(document.data == to_bytes(text));
    CHECK(!parse_document({}, "Plaintext")); // IR name is not a format name.
    CHECK(!parse_document({}, "json-dump"));
    CHECK(parse_document({}, "markdown") && parse_document({}, "markdown").value->ir == "MDAST");
    CHECK(parse_document({}, "plaintext"));

    const auto registry = builtin_conversions();
    CHECK(registry.supports("Plaintext", "Plaintext"));
    CHECK(!registry.supports("Plaintext", "DOM"));
    CHECK(registry.translate(document, "Plaintext").value->data == document.data);
    CHECK(registry.translate(document, "DOM").error.code == ErrorCode::unsupported_conversion);
    CHECK(registry.translate(document, "").error.code == ErrorCode::invalid_argument);
    auto unknown = document;
    unknown.ir = "Unknown";
    CHECK(!registry.translate(unknown, "Unknown")); // No invented identity routes.
    CHECK(!serialize_document(unknown, "plaintext"));
    CHECK(!serialize_document(document, "pdf"));
    document.encoding = "ascii";
    CHECK(!serialize_document(document, "json-dump"));
    document.encoding = "utf-8";
    CHECK(serialize_document(document, "plaintext").value == std::optional{document.data});
    CHECK(bytes_to_string(*serialize_document(document, "json-dump").value).find("\\u0000\\r\\n") != std::string::npos);
    CHECK(serialize_document(document, "xml-dump").error.code == ErrorCode::serialization_error);

    document.data = to_bytes("<&>\"'\r\n\t\xc3\xa9");
    document.metadata = {{"z", "last"}, {"a\n", "<&>\"'\r\n\t"}};
    const auto json = serialize_document(document, "json-dump");
    CHECK(json);
    CHECK(bytes_to_string(*json.value) ==
        "{\"ir\":\"Plaintext\",\"media_type\":\"text/plain\",\"encoding\":\"utf-8\","
        "\"text\":\"<&>\\\"'\\r\\n\\t\xc3\xa9\",\"metadata\":{\"a\\n\":\"<&>\\\"'\\r\\n\\t\",\"z\":\"last\"}}\n");
    const auto xml = serialize_document(document, "xml-dump");
    CHECK(xml);
    CHECK(bytes_to_string(*xml.value) ==
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<document ir=\"Plaintext\" media_type=\"text/plain\" encoding=\"utf-8\">"
        "<text>&lt;&amp;&gt;&quot;&apos;&#13;&#10;&#9;\xc3\xa9</text><metadata>"
        "<entry key=\"a&#10;\">&lt;&amp;&gt;&quot;&apos;&#13;&#10;&#9;</entry><entry key=\"z\">last</entry></metadata></document>\n");
    auto reordered = document;
    reordered.metadata.clear();
    reordered.metadata.emplace("a\n", "<&>\"'\r\n\t");
    reordered.metadata.emplace("z", "last");
    CHECK(serialize_document(reordered, "json-dump").value == json.value);
    CHECK(serialize_document(reordered, "xml-dump").value == xml.value);
    CHECK(serialize_document(document, "plaintext").error.code == ErrorCode::unsupported_conversion);
    CHECK(registry.translate(document, "Plaintext").value->metadata == document.metadata);
    document.metadata["invalid"] = "\xff";
    CHECK(!serialize_document(document, "json-dump"));
    CHECK(!serialize_document(document, "xml-dump"));
    document.metadata.clear();
    document.metadata[std::string(1, '\0')] = "value";
    CHECK(serialize_document(document, "json-dump"));
    CHECK(!serialize_document(document, "xml-dump"));
    document.metadata.clear();
    for (const std::string_view invalid_xml : {"\x01", "\xef\xbf\xbe", "\xef\xbf\xbf"}) {
        document.data = to_bytes(invalid_xml);
        CHECK(!serialize_document(document, "xml-dump"));
        CHECK(serialize_document(document, "json-dump"));
    }
    document.data = to_bytes("\xf4\x8f\xbf\xbf");
    CHECK(serialize_document(document, "xml-dump"));
    document.media_type = "\xff";
    CHECK(!serialize_document(document, "json-dump"));

    ConversionRegistry custom;
    CHECK(custom.add("", "B", [](const Document& d, const ConvertOptions&) { return Result<Document>::success(d); }));
    CHECK(custom.add("A", "B", {}));
    bool called = false;
    CHECK(!custom.add("A", "B", [&called](const Document& d, const ConvertOptions& options) {
        CHECK(options.allow_lossy);
        called = true;
        auto output = d;
        output.ir = "B";
        return Result<Document>::success(std::move(output));
    }));
    CHECK(custom.add("A", "B", [](const Document& d, const ConvertOptions&) { return Result<Document>::success(d); }));
    Document a;
    a.ir = "A";
    ConvertOptions lossy;
    lossy.allow_lossy = true;
    CHECK(custom.translate(a, "B", lossy));
    CHECK(called);
    CHECK(!custom.add("B", "C", [](const Document& d, const ConvertOptions&) { return Result<Document>::success(d); }));
    CHECK(custom.translate(a, "C").error.code == ErrorCode::unsupported_conversion);
    a.ir = "B";
    CHECK(custom.translate(a, "C").error.code == ErrorCode::internal_error);
    CHECK(!custom.add("B", "D", [](const Document&, const ConvertOptions&) {
        return Result<Document>::failure(ErrorCode::parse_error, "callback failed", "B");
    }));
    CHECK(custom.translate(a, "D").error.message == "callback failed");

    ConvertOptions options;
    CHECK(!convert({}, options));
    options.source_format = "plaintext";
    options.target_format = "plaintext";
    CHECK(*convert(to_bytes(text), options).value == to_bytes(text));
    CHECK(convert({}, options).value->empty());
    options.source_encoding = "iso-8859-1";
    CHECK(*convert(to_bytes("\xe9"), options).value == to_bytes("\xc3\xa9"));
    options.target_format = "json-dump";
    CHECK(convert({}, options));
    options.source_format = "xml-dump";
    CHECK(!convert({}, options));
    options.source_format = "plaintext";
    options.target_format = "pdf";
    CHECK(!convert({}, options));
    options.allow_lossy = true;
    options.strict = false;
    CHECK(!convert({}, options)); // Options do not invent unsupported capabilities.
    CHECK(builtin_formats().size() >= 6);
    CHECK(find_format("json-dump") && !find_format("json-dump")->readable);
    CHECK(!find_format("Plaintext"));

    const std::array<std::string_view, 6> arguments{"--to", "json-dump", "--encoding", "ascii", "in", "out"};
    const auto cli = parse_cli(arguments);
    CHECK(cli);
    CHECK(cli.value->input == "in" && *cli.value->output == "out");
    CHECK(cli.value->conversion.source_encoding == "ascii");
    CHECK(!parse_cli(std::array<std::string_view, 1>{"in"}));
    CHECK(!parse_cli(std::array<std::string_view, 1>{"--to"}));
    CHECK(!parse_cli(std::array<std::string_view, 2>{"--to", "--help"}));
    CHECK(!parse_cli(std::array<std::string_view, 1>{"--unknown"}));
    CHECK(!parse_cli(std::array<std::string_view, 5>{"--to", "plaintext", "--to", "json-dump", "in"}));
    CHECK(!parse_cli(std::array<std::string_view, 3>{"--to", "plaintext", "in"}, "json-dump"));
    CHECK(parse_cli(std::array<std::string_view, 1>{"--help"}));
    CHECK(parse_cli(std::array<std::string_view, 2>{"--", "-in"}, "json-dump").value->input == "-in");
}
