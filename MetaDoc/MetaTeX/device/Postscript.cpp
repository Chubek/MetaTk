#include "MetaTeX-Device.h"
#include "TextLayout.hpp"
#include "Vector.h"
namespace metatex {
std::string renderPostScript(const Document& doc) {
    auto out = detail::vectorStream(); out << std::fixed << std::setprecision(5);
    out << "%!PS-Adobe-3.0\n%%Creator: MetaTeX\n%%Pages: (atend)\n%%BoundingBox: 0 0 595 842\n%%LanguageLevel: 2\n%%EndComments\n"
        << "/MTFont {findfont dup length dict begin {1 index /FID ne {def}{pop pop} ifelse} forall\n"
        << "/Encoding ISOLatin1Encoding def currentdict end definefont pop} bind def\n"
        << "/MTR /Helvetica MTFont\n/MTB /Helvetica-Bold MTFont\n%%EndProlog\n";
    int pages = 0; double y = 786;
    auto page = [&] { if (pages) out << "showpage\n"; ++pages; out << "%%Page: " << pages << ' ' << pages << '\n'; y = 786; }; page();
    auto line = [&](std::string_view text, double size, bool bold, double left) {
        if (y - size * 1.4 < 55) page();
        out << '/' << (bold ? "MTB" : "MTR") << " findfont " << size << " scalefont setfont\n"
            << left << ' ' << y << " moveto (" << detail::psString(text) << ") show\n"; y -= size * 1.4;
    };
    for (const auto& b : doc.blocks) {
        if (b.kind == BlockKind::PageBreak) { page(); continue; }
        if (b.kind == BlockKind::HorizontalRule) {
            if (y < 70) page();
            out << "newpath 64 " << y << " moveto 531 " << y << " lineto 0 setgray 0.5 setlinewidth stroke\n"; y -= 20; continue;
        }
        if (b.kind == BlockKind::VectorGraphic) {
            auto scene = detail::sceneOf(b); double scale = detail::graphicScale(b, 467, 700), height = scene.height * scale;
            if (y - height < 55) page();
            out << "gsave 64 " << y << " translate " << scale << ' ' << -scale << " scale\n0 0 " << scene.width << ' ' << scene.height << " rectclip\n";
            for (std::size_t i = 0; i < scene.count; ++i) {
                const auto& path = scene.paths[i]; out << "newpath\n";
                for (std::size_t j = 0; j < path.count; ++j) {
                    const auto& c = path.commands[j]; int n = c.op == MV_CUBIC ? 6 : c.op == MV_CLOSE ? 0 : 2;
                    for (int k = 0; k < n; ++k) out << c.values[k] << ' ';
                    out << (c.op == MV_MOVE ? "moveto" : c.op == MV_LINE ? "lineto" : c.op == MV_CUBIC ? "curveto" : "closepath") << '\n';
                }
                if (path.fill.enabled) out << "gsave " << path.fill.red << ' ' << path.fill.green << ' ' << path.fill.blue << " setrgbcolor fill grestore\n";
                if (path.stroke.enabled) out << path.stroke.red << ' ' << path.stroke.green << ' ' << path.stroke.blue << " setrgbcolor " << path.stroke_width << " setlinewidth stroke\n";
                else out << "newpath\n";
            }
            out << "grestore\n"; y -= height + 16;
        }
        if (b.kind == BlockKind::TableOfContents) {
            line("Contents", 16, true, 64);
            for (const auto& h : doc.blocks) if (h.kind == BlockKind::Heading && h.level > 0)
                for (const auto& text : detail::textLines(detail::blockText(doc, h), 72)) line(text, 11, false, 76);
            continue;
        }
        bool heading = b.kind == BlockKind::Heading;
        double size = heading ? (b.level <= 0 ? 22 : b.level == 1 ? 18 : 14) : b.kind == BlockKind::Code ? 10 : 11;
        double left = b.kind == BlockKind::Quote || b.kind == BlockKind::Note ? 84 : 64;
        std::string text = detail::blockText(doc, b);
        if (b.kind == BlockKind::ListItem) text = (b.ordered ? "1. " : "* ") + text;
        for (const auto& part : detail::textLines(text, static_cast<std::size_t>((531 - left) / (size * 0.55)))) line(part, size, heading, left);
        y -= 9;
    }
    out << "showpage\n%%Trailer\n%%Pages: " << pages << "\n%%EOF\n"; return out.str();
}
}
