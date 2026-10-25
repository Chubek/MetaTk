#include "MetaTeX.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>

#define CHECK(x) do { if (!(x)) throw std::runtime_error("check failed at line " + std::to_string(__LINE__) + ": " #x); } while (false)

template<class F> void invalid(F fn) {
    try { fn(); } catch (const std::invalid_argument&) { return; }
    throw std::runtime_error("expected invalid_argument");
}

int main(int argc, char** argv) {
    using namespace metatex;
    using namespace metatex::cpp;
    using namespace metatex::cpp::literals;
    CHECK(argc == 2);
    const auto dir = std::filesystem::path(argv[1]);
    std::filesystem::create_directories(dir / "sub");

    // Exact TeX input and native construction must agree in IR and all devices.
    const auto parsed = metatex::compile(R"(Hello\textbf{bold}\textit{italic}$x^2$\par Next)");
    const auto native = document(
        paragraph("Hello"_text, bold("bold"), italic("italic"), "x^2"_math),
        paragraph("Next"));
    CHECK(!hasErrors(parsed));
    CHECK(native.blocks.size() == 2);
    CHECK(native.blocks[0].kind == BlockKind::Paragraph);
    CHECK(native.blocks[0].spans.size() == 4);
    CHECK(native.blocks[0].spans[1].kind == SpanKind::Bold);
    CHECK(native.blocks[0].spans[1].text == "bold");
    CHECK(native.blocks[0].spans[3].kind == SpanKind::Math);
    CHECK(native.blocks[0].spans[3].text == "x^2");
    CHECK(native.blocks[1].spans[0].text == "Next");
    CHECK(renderJSON(native) == renderJSON(parsed));
    CHECK(renderHTML(native) == renderHTML(parsed));
    CHECK(renderHTML(native).find("Hello<strong>bold</strong><em>italic</em><math") != std::string::npos);
    CHECK(renderPDF(native) == renderPDF(parsed));
    CHECK(renderPDF(native).find("(Hellobolditalicx^2) Tj") != std::string::npos);
    CHECK(renderText(native) == "Hellobolditalicx^2\n\nNext\n\n");

    // Factories copy string views, do not interpret TeX, and preserve whitespace.
    std::string input = "  <&> \\undefined {x} $y$\n";
    const auto literal = paragraph(std::string_view(input));
    input.clear();
    CHECK(literal.spans[0].text == "  <&> \\undefined {x} $y$\n");
    CHECK(renderHTML(document(literal)).find("  &lt;&amp;&gt; \\undefined {x} $y$ ") != std::string::npos);
    CHECK("a\0b"_text.text.size() == 3);

    const auto base = document(heading(1, "Intro") | withId("intro"), paragraph(ref("intro")))
        | title("A & B") | author("Author") | date("2026-10-25") | reference("intro", "1");
    CHECK(base.title == "A & B" && base.author == "Author" && base.date == "2026-10-25");
    CHECK(base.blocks[0].id == "intro" && base.blocks[0].level == 1);
    CHECK(base.references.at("intro") == "1");
    CHECK(renderHTML(base).find("<h2 id=\"intro\">Intro</h2>") != std::string::npos);
    CHECK(renderHTML(base).find("<span class=\"ref\">1</span>") != std::string::npos);
    CHECK(renderText(base) == "# Intro\n\n1\n\n");
    CHECK(renderPDF(base).find("(1) Tj") != std::string::npos);

    // ADL also finds std::ref for std::string arguments. The DSL must still
    // produce an owning reference span for every string value category.
    std::string dynamicKey = "intro";
    const std::string constKey = dynamicKey;
    static_assert(std::same_as<decltype(ref(dynamicKey)), Span>);
    static_assert(std::same_as<decltype(ref(constKey)), Span>);
    static_assert(std::same_as<decltype(ref(std::string("intro"))), Span>);
    static_assert(std::same_as<decltype(ref(std::move(constKey))), Span>);
    auto dynamicRefs = document(paragraph(ref(dynamicKey), ref(constKey), ref(std::string("intro"))))
        | reference("intro", "1");
    dynamicKey.clear();
    CHECK(renderText(dynamicRefs) == "111\n\n");
    CHECK(dynamicRefs.blocks[0].spans[0].extra == "ref:intro");
    CHECK(renderHTML(dynamicRefs).find("<p><span class=\"ref\">1</span><span class=\"ref\">1</span><span class=\"ref\">1</span></p>") != std::string::npos);
    CHECK(renderPDF(dynamicRefs).find("(111) Tj") != std::string::npos);

    DocumentBuilder builder(base);
    builder.title("Builder").author("Someone").date("Today").reference("intro", "I")
        .append(paragraph("tail"));
    const auto snapshot = builder.build();
    builder.append(pageBreak()).append(paragraph("last"));
    const auto built = std::move(builder).build();
    CHECK(base.blocks.size() == 2 && base.title == "A & B");
    CHECK(snapshot.blocks.size() == 3 && snapshot.references.at("intro") == "I");
    CHECK(snapshot.title == "Builder" && snapshot.author == "Someone" && snapshot.date == "Today");
    CHECK(built.blocks.size() == 5);
    CHECK(renderPDF(built).find("/Count 2") != std::string::npos);
    const auto addTail = append(paragraph("tail"));
    const auto extended = base | addTail;
    const auto reused = base | addTail;
    CHECK(extended.blocks.size() == 3 && extended.blocks.back().spans[0].text == "tail");
    CHECK(renderJSON(extended) == renderJSON(reused));
    CHECK(base.blocks.size() == 2);
    CHECK((base | ::dsl::pipe([](const Document& doc) { return doc.blocks.size(); })) == 2);

    const auto all = document(
        paragraph(code("c"), smallCaps("s"), underline("u"),
                  link("site", "https://example.org/?x=1&y=2"), image("alt", "image.png")),
        listItem(2, true, "ordered"), quote("quoted"), note("noted"),
        codeBlock("a\n<b>"), displayMath(R"(\frac{1}{2})"), horizontalRule(),
        tableOfContents(), pageBreak(), heading(0, "End"));
    CHECK(all.blocks.size() == 10);
    CHECK(all.blocks[1].kind == BlockKind::ListItem && all.blocks[1].level == 2 && all.blocks[1].ordered);
    CHECK(all.blocks[5].spans[0].kind == SpanKind::Math);
    const auto html = renderHTML(all);
    for (const auto* expected : {"<code>c</code>", "<span class=\"smallcaps\">s</span>", "<u>u</u>",
             "href=\"https://example.org/?x=1&amp;y=2\"", "src=\"image.png\" alt=\"alt\"",
             "1. ordered", "<blockquote>quoted</blockquote>", "<aside class=\"note\">noted</aside>",
             "<pre><code>a\n&lt;b&gt;</code></pre>", "<mfrac>", "<hr>", "Table of contents",
             "break-before:page", "<h1 id=\"\">End</h1>"})
        CHECK(html.find(expected) != std::string::npos);
    CHECK(renderPDF(all).find("/Count 2") != std::string::npos);
    CHECK(renderJSON(all).find("\"type\": \"display-math\"") != std::string::npos);

    const auto vectors = document(graphic(metavect::Scene(
        "metavect-scene 1 20 10; stroke 0 0 1; rect 1 2 3 4;"), "Caption", 40));
    CHECK(vectors.blocks[0].graphicWidth == 40);
    CHECK(vectors.blocks[0].graphic->view().count == 1);
    CHECK(renderHTML(vectors).find("width:40pt") != std::string::npos);
    CHECK(renderPDF(vectors).find("2.00000000 0 0 -2.00000000") != std::string::npos);
    CHECK(renderJSON(vectors).find("\"type\": \"vector-graphic\"") != std::string::npos);

    // Existing handlers, filename-relative input, and diagnostics remain usable.
    Engine engine;
    engine.registerCommand("native", [](Engine& e) { e.addSpan(SpanKind::Bold, e.readTextArgument()); });
    auto compileStage = compileWith(engine, dir / "main.tex");
    CHECK(renderJSON(R"(\native{bold})" | compileStage) == renderJSON(document(paragraph(bold("bold")))));
    CHECK((R"(\native{again})" | compileStage | renderAs(OutputFormat::Text)) == "again\n\n");
    writeFile(dir / "sub" / "part.tex", "Included");
    writeFile(dir / "main.tex", R"(\input{sub/part})");
    CHECK(renderText(compileFile(engine, dir / "main.tex")) == "Included\n\n");
    CHECK(renderText(compileFile(dir / "main.tex")) == "Included\n\n");
    CHECK((R"(\input{sub/part})" | compileStage | renderAs(OutputFormat::Text)) == "Included\n\n");
    auto failed = R"(\undefined)" | compileStage;
    CHECK(hasErrors(failed));
    CHECK(failed.diagnostics[0].file == (dir / "main.tex").string());
    DocumentBuilder diagnostics;
    diagnostics.diagnostic({Diagnostic::Severity::Warning, "warning", "native.cpp", 1, 2});
    CHECK(!hasErrors(diagnostics.build()));
    diagnostics.diagnostic({Diagnostic::Severity::Error, "error", "native.cpp", 3, 4});
    CHECK(hasErrors(diagnostics.build()));

    for (auto format : {OutputFormat::HTML, OutputFormat::PDF, OutputFormat::JSON, OutputFormat::Text}) {
        const auto path = dir / (std::to_string(static_cast<int>(format)) + ".out");
        const auto output = built | renderAs(format);
        writeDocument(built, path, format);
        CHECK(readFile(path) == output);
        auto written = built | writeTo(path, format);
        CHECK(readFile(path) == output && renderJSON(written) == renderJSON(built));
    }
    CHECK((native | renderAs(OutputFormat::PDF)).starts_with("%PDF-1.4"));
    CHECK((native | renderAs(OutputFormat::PDF)).ends_with("%%EOF\n"));
    CHECK(document().blocks.empty() && paragraph().spans.empty());
    invalid([] { heading(-1, "bad"); });
    invalid([] { heading(6, "bad"); });
    invalid([] { listItem(0, false, "bad"); });
    for (auto width : {-1.0, 0.001, 1000001.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()})
        invalid([&] { graphic(metavect::Scene("metavect-scene 1 20 10;"), "bad", width); });
    invalid([&] { render(native, static_cast<OutputFormat>(-1)); });
    std::cout << "C++ API and native DSL tests passed\n";
}
