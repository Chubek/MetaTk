#include "DoxTk.hpp"
#include "doxtk/DoxTk-Bytestream.hpp"
#include "test_support.hpp"

int main() {
    using namespace doxtk;
    cppdsl::DocumentDSL d;
    auto tree = d.document({d.heading(2, {d.text("é")}),
        d.paragraph({d.text("hello "), d.emphasis({d.text("world")}), d.line_break(),
            d.link("https://example.test", {d.text("label")}, "title"), d.image("alt", "img")}),
        d.blockquote({d.paragraph({d.inline_code("code")})}),
        d.list({d.item({d.paragraph({d.text("item")})}, true)}, true, 3),
        d.code(std::string("a\0b", 3), "cpp"), d.thematic_break(), d.verbatim("raw", "literal")});
    CHECK(tree);
    auto built = d.wrap(std::move(tree)) | d.finish({{"author", "é"}});
    CHECK(built && built.value->tree && built.value->data.empty());
    CHECK(built.value->ir == "MDAST" && !built.value->tree->start.valid);
    auto registry = builtin_conversions();
    CHECK(registry.translate(*built.value, "DoxTkJSON"));
    CHECK(!registry.translate(*built.value, "DoxTkXML")); // NUL is retained, not filtered.
    CHECK(!registry.translate(*built.value, "Plaintext"));
    ConvertOptions options; options.allow_lossy = true;
    auto plain = registry.translate(*built.value, "Plaintext", options);
    CHECK(plain && !plain.diagnostics.empty() && plain.value->metadata == built.value->metadata);
    CHECK(bytes_to_string(plain.value->data).find("hello world") != std::string::npos);
    CHECK(!d.heading(0)); CHECK(!d.heading(7));
    CHECK(!d.document({d.text("not a block")}));
    CHECK(!d.paragraph({d.code("not a phrase")}));
    CHECK(!d.list({d.paragraph({})}));
    CHECK(!d.text(std::string(1, '\xff')));
    CHECK(!d.image("alt", "\xff"));
    CHECK(!d.build(d.paragraph({})));
    CHECK(!d.build(Result<Node>{}));
    CHECK(d.document({Result<Node>{}}).error.code == ErrorCode::internal_error);
    CHECK(!d.build(d.document({}), {{"invalid", "\xff"}}));
    const auto failure = d.wrap(d.document({d.paragraph({d.text("\xff")})})) | d.finish();
    CHECK(!failure && failure.error.code == ErrorCode::parse_error);
    auto empty = d.build(d.document({}));
    CHECK(empty && empty.value->tree->children.empty());
    CHECK(registry.translate(*empty.value, "DoxTkXML"));
    auto child = d.text("warning"); child.diagnostics.push_back("retained");
    CHECK(d.build(d.document({d.paragraph({child})})).diagnostics == child.diagnostics);
    auto deep = d.paragraph({d.text("leaf")});
    for (int i = 0; i < 260; ++i) deep = d.blockquote({deep});
    CHECK(!deep);
}
