#include "../../../device/DVI.hpp"
#include <iostream>
#include <string>

namespace {
void jsonString(std::ostream& out, const std::string& value) {
    out << '"';
    for (unsigned char c : value) {
        if (c == '"' || c == '\\') out << '\\' << static_cast<char>(c);
        else if (c >= 32 && c < 127) out << static_cast<char>(c);
        else {
            constexpr char hex[] = "0123456789abcdef";
            out << "\\u00" << hex[c >> 4] << hex[c & 15];
        }
    }
    out << '"';
}
} // namespace

int main(int argc, char** argv) {
    bool json = argc == 3 && std::string(argv[1]) == "--json";
    if ((argc != 2 && !json) || (argc == 2 && std::string(argv[1]) == "--help")) {
        std::cerr << "usage: texscrape-dvi [--json] FILE.dvi\n";
        return argc == 2 && std::string(argv[1]) == "--help" ? 0 : 2;
    }
    try {
        auto doc = metatex::dvi::parseFile(argv[json ? 2 : 1]);
        if (json) {
            std::cout << "{\"pages\":" << doc.pages.size()
                      << ",\"magnification\":" << doc.magnification
                      << ",\"numerator\":" << doc.numerator
                      << ",\"denominator\":" << doc.denominator
                      << ",\"max_stack_depth\":" << doc.maximum_stack_depth
                      << ",\"comment\":";
            jsonString(std::cout, doc.comment);
            std::cout << ",\"fonts\":[";
            for (std::size_t i = 0; i < doc.fonts.size(); ++i) {
                if (i) std::cout << ',';
                std::cout << "{\"id\":" << doc.fonts[i].id << ",\"name\":";
                jsonString(std::cout, doc.fonts[i].area + doc.fonts[i].name);
                std::cout << '}';
            }
            std::cout << "],\"page_details\":[";
            for (std::size_t i = 0; i < doc.pages.size(); ++i) {
                if (i) std::cout << ',';
                std::cout << "{\"offset\":" << doc.pages[i].offset
                          << ",\"glyphs\":" << doc.pages[i].glyphs << '}';
            }
            std::cout << "]}\n";
        } else {
            std::cout << "DVI version 2; pages: " << doc.pages.size()
                      << "; magnification: " << doc.magnification
                      << "; font definitions: " << doc.fonts.size() << '\n';
            for (const auto& page : doc.pages)
                std::cout << "page at byte " << page.offset << ": "
                          << page.glyphs << " glyphs, " << page.operations
                          << " operations, stack " << page.max_stack_depth << '\n';
        }
    } catch (const std::exception& e) {
        std::cerr << "TeXScrape DVI: " << e.what() << '\n';
        return 1;
    }
}
