#pragma once
#include "FontLoader.hpp"
#include <string_view>
namespace metatex::fonts {
enum class Direction { Auto, LeftToRight, RightToLeft, TopToBottom, BottomToTop };
struct ShapeOptions {
    double pointSize = 12;
    Direction direction = Direction::Auto;
    std::string language, script;
    std::vector<std::string> features; // HarfBuzz syntax, e.g. "liga=0".
};
struct Glyph {
    std::uint32_t id = 0, cluster = 0; // Cluster is a UTF-8 byte offset.
    double xAdvance = 0, yAdvance = 0, xOffset = 0, yOffset = 0; // Points.
};
struct ShapedRun {
    std::vector<Glyph> glyphs;
    Direction direction = Direction::LeftToRight;
    double xAdvance = 0, yAdvance = 0;
    bool missingGlyphs = false;
};
// Shapes a single directional/script run. Caller performs paragraph bidi
// segmentation and font fallback; no silent replacement of missing glyphs.
ShapedRun shape(const Font& font, std::string_view utf8, const ShapeOptions& options = {});
}
