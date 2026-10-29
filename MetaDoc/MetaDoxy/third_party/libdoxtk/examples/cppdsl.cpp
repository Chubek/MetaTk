#include <DoxTk.hpp>
#include <doxtk/DoxTk-Bytestream.hpp>
#include <iostream>

int main() {
    doxtk::cppdsl::DocumentDSL d;
    auto built = d.wrap(d.document({
        d.heading(1, {d.text("DoxTk")}),
        d.paragraph({d.text("Hello "), d.strong({d.text("world")}), d.text("!")}),
        d.list({d.item({d.paragraph({d.text("Native C++")})})})
    })) | d.finish({{"author", "example"}});
    if (!built) { std::cerr << built.error.message << '\n'; return 1; }
    auto output = doxtk::builtin_conversions().translate(*built.value, "DoxTkJSON");
    if (!output) { std::cerr << output.error.message << '\n'; return 1; }
    std::cout << doxtk::bytes_to_string(output.value->data);
    return std::cout ? 0 : 1;
}
