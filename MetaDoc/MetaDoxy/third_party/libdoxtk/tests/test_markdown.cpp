#include "DoxTk.hpp"
#include "doxtk/DoxTk-Bytestream.hpp"
#include "test_support.hpp"
#include <algorithm>

using namespace doxtk;

Node parse(std::string_view source) {
    auto result = mdast::parse(source);
    CHECK(result);
    CHECK(!result.value->validate());
    return std::move(*result.value);
}

std::string plain(std::string_view source) {
    auto result = mdast::to_plaintext(parse(source));
    CHECK(result && !result.diagnostics.empty());
    return *result.value;
}

int main() {
    const auto fixture = read_file("basic.md");
    const auto expected = read_file("basic.txt");
    CHECK(fixture && expected);
    CHECK(plain(bytes_to_string(*fixture.value)) + "\n" == bytes_to_string(*expected.value));
    CHECK(parse("").children.empty());
    CHECK(parse(" \r\n\t\n").children.empty());
    CHECK(plain("a\r\nb\rc\n") == "a\nb\nc");
    CHECK(plain("Hello *world* and **friends**.") == "Hello world and friends.");
    CHECK(plain("*one **two** three*") == "one two three");
    CHECK(plain("*one* *two* `code` ![alt](url) [label](url)") == "one two code alt label");
    CHECK(plain("a  \nb\\\nc") == "a\nb\nc");
    CHECK(plain("- one\n- two") == "- one\n- two");
    CHECK(plain("> *hello*\n>\n> - item") == "hello\n\n- item");
    CHECK(plain("[ref]: url\nordinary text") == "[ref]: url\n\nordinary text");
    CHECK(plain("[ref]") == "[ref]");
    CHECK(plain("![unterminated") == "![unterminated");
    CHECK(plain("[label]") == "[label]"); // Previously read beyond closing bracket.
    CHECK(plain("**") == "**");
    CHECK(plain("`a``b") == "`a``b");
    CHECK(plain("`a``b`") == "a``b");
    CHECK(plain("``a```b``") == "a```b");
    CHECK(parse("`a``b").children[0].children[0].kind == "text");
    CHECK(plain("x*\tword* word*\t*") == "x*\tword* word*\t*");
    CHECK(plain("_word_part_") == "word_part");
    CHECK(plain("_word_part") == "_word_part");
    CHECK(plain("(_word_)") == "(word)");
    CHECK(plain("[label](a(b)c)") == "[label](a(b)c)");
    CHECK(plain("[label](bad url)") == "[label](bad url)");
    auto titled = parse("[label](url \"a ) title\")").children[0].children[0];
    CHECK(titled.kind == "link" && titled.attribute_equals("title", "a ) title"));
    CHECK(titled.attribute_equals("url", "url"));
    CHECK(plain("[label](url \"unterminated)") == "[label](url \"unterminated)");
    CHECK(parse("a\\\\\nb").children[0].children[1].kind == "text");
    CHECK(parse("a\\\nb").children[0].children[1].kind == "break");
    CHECK(plain("***unimplemented***") == "***unimplemented***");
    CHECK(plain("a_b_c") == "a_b_c");
    CHECK(plain("é *文* 😀") == "é 文 😀");
    CHECK(plain("paragraph\n# heading") == "paragraph\n\nheading");
    CHECK(parse("#\n## ").children.size() == 2); // Empty headings are real nodes.
    CHECK(plain("# title ###") == "title");
    const auto ordered = parse("12. first\n13. second");
    CHECK(ordered.children[0].attribute_equals("ordered", "true"));
    CHECK(ordered.children[0].attribute_equals("start", "12"));
    CHECK(ordered.children[0].children[0].children[0].kind == "paragraph");
    CHECK(parse("- [x] done").children[0].children[0].attribute_equals("checked", "true"));
    CHECK(parse("- [ ] todo").children[0].children[0].attribute_equals("checked", "false"));
    CHECK(parse("-*-\n").children[0].kind != "thematicBreak");
    CHECK(parse("- - -").children[0].kind == "thematicBreak");
    CHECK(parse("    # code").children[0].kind == "verbatim");
    CHECK(parse("\t```\n").children[0].kind == "verbatim");
    auto code = parse("````cpp meta\n\nfirst\n```\nlast\n````\n").children[0];
    CHECK(code.text == "\nfirst\n```\nlast");
    CHECK(code.attribute_equals("fenced", "true"));
    CHECK(code.attribute_equals("lang", "cpp"));
    CHECK(code.attribute_equals("meta", "meta"));
    CHECK(parse("```\n\n```\n").children[0].text.empty());
    CHECK(parse("```\nlast\n").children[0].text == "last");
    CHECK(parse("> [ref]: url").children[0].children[0].kind == "verbatim");
    CHECK(!mdast::parse(std::string(1, '\xff')));
    CHECK(!mdast::parse(std::string(70, '>') + " text"));
    Node malformed;
    malformed.kind = "text";
    malformed.children.push_back(parse("child"));
    CHECK(malformed.validate());

    const auto positioned = parse("# é\r\n\r\ntext");
    CHECK(positioned.end.line == 2 && positioned.end.column == 4 && positioned.end.offset == 12);
    CHECK(positioned.children[0].children[0].start.column == 2);
    CHECK(positioned.children[0].children[0].end.column == 3);
    CHECK(positioned.children[1].start.line == 2 && positioned.children[1].start.offset == 8);
    auto node = parse("hello");
    node.attributes = {{"a key with spaces", "<value>"}, {"kind", "not markup"}};
    std::unordered_map<std::string, std::string> metadata{{"z", "last"}, {"a", "<&>\n"}};
    auto json = to_ir_json(node, "MDAST", "text/markdown", metadata);
    auto xml = to_ir_xml(node, "MDAST", "text/markdown", metadata);
    CHECK(json && xml);
    CHECK(json.value->find("\"metadata\":{\"a\":\"<&>\\n\",\"z\":\"last\"}") != std::string::npos);
    CHECK(xml.value->find("<entry key=\"a\">&lt;&amp;&gt;&#10;</entry>") != std::string::npos);
    CHECK(xml.value->find("<attribute key=\"a key with spaces\">&lt;value&gt;</attribute>") != std::string::npos);
    CHECK(xml.value->find("<start valid=\"true\" line=\"0\" column=\"0\" offset=\"0\"/>") != std::string::npos);
    const auto original_json = json.value;
    metadata.clear(); metadata.emplace("a", "<&>\n"); metadata.emplace("z", "last");
    CHECK(to_ir_json(node, "MDAST", "text/markdown", metadata).value == original_json);
    node.children[0].children[0].text = std::string(1, '\xff');
    CHECK(!to_ir_json(node, "MDAST", "text/markdown", metadata));
    CHECK(!to_ir_xml(node, "MDAST", "text/markdown", metadata));
    node = parse(std::string("a\0b", 3));
    CHECK(to_ir_json(node, "MDAST", "text/markdown", {}));
    CHECK(!to_ir_xml(node, "MDAST", "text/markdown", {}));
    node = parse("x");
    CHECK(!to_ir_json(node, "\xff", "", {}));
    CHECK(!to_ir_xml(node, "\xf0", "", {}));
    node.end.valid = false;
    CHECK(node.validate());
    node = parse("x");
    node.end.line = 1;
    node.end.offset = 0;
    CHECK(node.validate());
    node = parse("x");
    node.children[0].end.line = 2;
    CHECK(node.validate());
    node = parse(std::string("line\n\"quote\"\t\0", 14));
    const auto outline = node.outline();
    CHECK(static_cast<std::size_t>(std::count(outline.begin(), outline.end(), '\n')) == node.count_descendants() + 1);
    CHECK(outline.find("\\\"quote\\\"\\t\\x00") != std::string::npos);
    node = parse("x");
    node.start.column = 2;
    CHECK(node.validate());
    node = parse("x");
    node.children[0].end.offset = 99;
    CHECK(node.validate());
    node = parse("x");
    node.attributes = {{"z", ""}, {"a", ""}};
    CHECK(node.validate());

    struct Visitor : ASTVisitor {
        std::string text;
        std::size_t entered = 0, left = 0;
        void enter(const Node&) override { ++entered; }
        void leave(const Node&) override { ++left; }
        bool visit_text(const Node& n) override { text += n.text; return true; }
    } visitor;
    const auto visited = parse("**bold**\n\n<raw>");
    CHECK(visitor.traverse(visited));
    CHECK(visitor.text == "bold");
    CHECK(visitor.entered == visited.count_descendants() + 1 && visitor.left == visitor.entered);
    struct Stop : ASTVisitor { bool visit_text(const Node&) override { return false; } } stop;
    CHECK(!stop.traverse(visited));

    auto document = parse_markdown(to_bytes("*visible*"));
    CHECK(document && document.value->tree && document.value->tree->children[0].kind == "paragraph");
    ConvertOptions options;
    options.source_format = "markdown";
    options.target_format = "plaintext";
    CHECK(!convert(to_bytes("*visible*"), options));
    options.allow_lossy = true;
    auto converted = convert(to_bytes("*visible*"), options);
    CHECK(converted && bytes_to_string(*converted.value) == "visible" && !converted.diagnostics.empty());
    const auto registry = builtin_conversions();
    document.value->metadata = {{"author", "someone"}};
    const auto translated = registry.translate(*document.value, "Plaintext", options);
    CHECK(translated && translated.value->metadata == document.value->metadata);
    CHECK(!serialize_document(*translated.value, "plaintext")); // No silent metadata dropping.
    auto dump = registry.translate(*document.value, "DoxTkJSON");
    CHECK(dump && bytes_to_string(dump.value->data).find("someone") != std::string::npos);
    CHECK(serialize_document(*dump.value, "ir-json"));
    options.allow_lossy = false;
    options.target_format = "ir-xml";
    CHECK(convert(to_bytes("**bold** [link](url \"title\")"), options));
    options.target_format = "ir-json";
    CHECK(convert({}, options));
    document.value->tree.reset(); // Compatibility with source-only envelopes.
    CHECK(registry.translate(*document.value, "DoxTkJSON"));
}
