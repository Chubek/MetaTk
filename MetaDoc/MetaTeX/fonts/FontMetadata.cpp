#include "FontMetadata.hpp"
#include "../utils/Memory.hpp"
#include <limits>
#include <stdexcept>
#include <ft2build.h>
#include FT_FREETYPE_H
namespace metatex::fonts {
FontMetadata inspectFont(std::span<const std::uint8_t> bytes, unsigned faceIndex) {
    if (bytes.empty() || bytes.size() > static_cast<std::size_t>(std::numeric_limits<FT_Long>::max()) || faceIndex > 65535)
        throw std::invalid_argument("invalid font data size or face index");
    FT_Library library = nullptr;
    if (FT_Init_FreeType(&library)) throw std::runtime_error("FreeType initialization failed");
    util::ScopeExit freeLibrary([&] { FT_Done_FreeType(library); });
    FT_Face face = nullptr;
    if (FT_New_Memory_Face(library, bytes.data(), static_cast<FT_Long>(bytes.size()), faceIndex, &face))
        throw std::runtime_error("cannot decode font face");
    util::ScopeExit freeFace([&] { FT_Done_Face(face); });
    if (!FT_IS_SCALABLE(face) || !face->units_per_EM) throw std::runtime_error("font is not scalable");
    FontMetadata result;
    result.family = face->family_name ? face->family_name : "";
    result.style = face->style_name ? face->style_name : "";
    if (const char* name = FT_Get_Postscript_Name(face)) result.postscriptName = name;
    result.faceIndex = faceIndex; result.faceCount = static_cast<unsigned>(face->num_faces);
    result.unitsPerEm = face->units_per_EM; result.glyphCount = face->num_glyphs;
    result.ascender = face->ascender; result.descender = face->descender; result.lineHeight = face->height;
    result.bold = (face->style_flags & FT_STYLE_FLAG_BOLD) != 0;
    result.italic = (face->style_flags & FT_STYLE_FLAG_ITALIC) != 0;
    result.fixedWidth = FT_IS_FIXED_WIDTH(face);
    return result;
}
}
