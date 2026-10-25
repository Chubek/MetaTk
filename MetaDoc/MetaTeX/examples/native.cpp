#include "MetaTeX.hpp"
#include <iostream>

int main() {
    using namespace metatex::cpp;
    using namespace metatex::cpp::literals;

    auto doc = document(
        heading(0, "A native C++ document"),
        tableOfContents(),
        heading(1, "Introduction") | withId("intro"),
        paragraph("Hello, "_text, bold("MetaTeX"), ". Here is ", "x^2"_math, "."),
        note("Built directly from C++ values."),
        paragraph("See section ", ref("intro"), ".")
    ) | title("Native MetaTeX") | author("C++ application")
      | reference("intro", "1");

    // The same factories work in an imperative API, including dynamic content.
    metatex::DocumentBuilder builder(std::move(doc));
    for (int i = 1; i <= 3; ++i)
        builder.append(listItem(1, false, "Item " + std::to_string(i)));

    std::cout << (std::move(builder).build() | renderAs(metatex::OutputFormat::HTML));
}
