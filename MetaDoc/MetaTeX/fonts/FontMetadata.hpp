#pragma once
#include <cstdint>
#include <span>
#include <string>
namespace metatex::fonts {
struct FontMetadata {
    std::string family, style, postscriptName;
    unsigned faceIndex = 0, faceCount = 0, unitsPerEm = 0;
    long glyphCount = 0;
    int ascender = 0, descender = 0, lineHeight = 0;
    bool bold = false, italic = false, fixedWidth = false;
};
// Inspect a scalable SFNT/Type1 face using FreeType. Invalid data throws.
FontMetadata inspectFont(std::span<const std::uint8_t> bytes, unsigned faceIndex = 0);
}
