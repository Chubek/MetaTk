#include "MetaTeX-Device.h"
#include "TextLayout.hpp"
#include "Vector.h"
namespace metatex {
std::string renderTroffIR(const Document& doc) {
    // Ditroff text IR in 1/72000 inch units. Equations and external images
    // retain their semantic source/description; vector blocks retain captions.
    auto out = detail::vectorStream();
    out << "x T ps\nx res 72000 1 1\nx init\nx font 1 TR\nx font 2 TB\n";
    int page = 0, y = 56000;
    auto newPage = [&] { out << 'p' << ++page << '\n'; y = 56000; }; newPage();
    for (const auto& block : doc.blocks) {
        if (block.kind == BlockKind::PageBreak) { newPage(); continue; }
        if (block.kind == BlockKind::HorizontalRule) {
            if (y > 780000) newPage();
            out << "H64000\nV" << y << "\nD l 467000 0\n"; y += 20000; continue;
        }
        auto text = detail::blockText(doc, block);
        if (block.kind == BlockKind::ListItem) text = (block.ordered ? std::to_string(std::max(1,block.listNumber))+". " : "* ") + text;
        if (block.kind == BlockKind::TableOfContents) {
            text = "Contents";
            for (const auto& h : doc.blocks) if (h.kind == BlockKind::Heading && h.level > 0) text += '\n' + detail::blockText(doc, h);
        }
        int size = block.kind == BlockKind::Heading ? (block.level == 0 ? 22 : 16) : 11;
        for (const auto& line : detail::textLines(text, static_cast<std::size_t>(467 / (size * .55)))) {
            if (y + size * 1400 > 787000) newPage();
            out << 's' << size * 1000 << "\nf" << (block.kind == BlockKind::Heading ? 2 : 1) << "\nH64000\nV" << y << '\n';
            for (std::size_t at = 0; at < line.size();) {
                auto code = util::nextUTF8(line, at);
                if (code == ' ') out << "w\n";
                else if (code >= 33 && code <= 126) out << 'c' << static_cast<char>(code) << '\n';
                else if (code >= 160) out << "C u" << std::uppercase << std::hex << std::setw(4) << std::setfill('0') << code << std::dec << std::nouppercase << '\n';
                else out << "c?\n";
                out << 'h' << size * 550 << '\n';
            }
            out << "n " << size * 1400 << " 0\n"; y += size * 1400;
        }
        y += 9000;
    }
    out << "x trailer\nx stop\n"; return out.str();
}
}
