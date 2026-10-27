#include "FontShaping.hpp"
#include "../utils/Memory.hpp"
#include "../utils/UTF8.hpp"
#include <cmath>
#include <limits>
#include <hb.h>
#include <hb-ot.h>
namespace metatex::fonts {
ShapedRun shape(const Font& source, std::string_view text, const ShapeOptions& options) {
    if (!std::isfinite(options.pointSize) || options.pointSize <= 0 || options.pointSize > 1000000)
        throw std::invalid_argument("invalid shaping point size");
    if (text.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
        source.bytes.size() > std::numeric_limits<unsigned>::max() || source.bytes.empty())
        throw std::length_error("shaping input size invalid");
    util::validateUTF8(text);
    auto* blob = hb_blob_create(reinterpret_cast<const char*>(source.bytes.data()),
        static_cast<unsigned>(source.bytes.size()), HB_MEMORY_MODE_READONLY, nullptr, nullptr);
    util::ScopeExit freeBlob([&] { hb_blob_destroy(blob); });
    auto* face = hb_face_create(blob, source.metadata.faceIndex);
    util::ScopeExit freeFace([&] { hb_face_destroy(face); });
    if (!hb_face_get_glyph_count(face)) throw std::runtime_error("font has no OpenType glyphs");
    auto* font = hb_font_create(face); util::ScopeExit freeFont([&] { hb_font_destroy(font); });
    hb_ot_font_set_funcs(font);
    auto upem = hb_face_get_upem(face); hb_font_set_scale(font, static_cast<int>(upem), static_cast<int>(upem));
    auto* buffer = hb_buffer_create(); util::ScopeExit freeBuffer([&] { hb_buffer_destroy(buffer); });
    hb_buffer_add_utf8(buffer, text.data(), static_cast<int>(text.size()), 0, static_cast<int>(text.size()));
    hb_direction_t direction = HB_DIRECTION_INVALID;
    switch (options.direction) {
    case Direction::Auto: break;
    case Direction::LeftToRight: direction = HB_DIRECTION_LTR; break;
    case Direction::RightToLeft: direction = HB_DIRECTION_RTL; break;
    case Direction::TopToBottom: direction = HB_DIRECTION_TTB; break;
    case Direction::BottomToTop: direction = HB_DIRECTION_BTT; break;
    }
    if (direction != HB_DIRECTION_INVALID) hb_buffer_set_direction(buffer, direction);
    if (!options.language.empty()) hb_buffer_set_language(buffer, hb_language_from_string(options.language.c_str(), -1));
    if (!options.script.empty()) {
        auto script = hb_script_from_string(options.script.c_str(), -1);
        if (script == HB_SCRIPT_INVALID || script == HB_SCRIPT_UNKNOWN) throw std::invalid_argument("unknown shaping script");
        hb_buffer_set_script(buffer, script);
    }
    hb_buffer_guess_segment_properties(buffer);
    std::vector<hb_feature_t> features;
    for (const auto& item : options.features) {
        hb_feature_t feature{};
        if (!hb_feature_from_string(item.c_str(), -1, &feature)) throw std::invalid_argument("invalid font feature: " + item);
        features.push_back(feature);
    }
    hb_shape(font, buffer, features.data(), static_cast<unsigned>(features.size()));
    if (!hb_buffer_allocation_successful(buffer)) throw std::bad_alloc();
    unsigned count = 0;
    const auto* info = hb_buffer_get_glyph_infos(buffer, &count);
    const auto* positions = hb_buffer_get_glyph_positions(buffer, nullptr);
    ShapedRun result; result.glyphs.reserve(count);
    direction = hb_buffer_get_direction(buffer);
    result.direction = direction == HB_DIRECTION_RTL ? Direction::RightToLeft : direction == HB_DIRECTION_TTB ?
        Direction::TopToBottom : direction == HB_DIRECTION_BTT ? Direction::BottomToTop : Direction::LeftToRight;
    const double scale = options.pointSize / upem;
    for (unsigned i = 0; i < count; ++i) {
        const auto& p = positions[i];
        Glyph glyph{info[i].codepoint, info[i].cluster, p.x_advance * scale, p.y_advance * scale, p.x_offset * scale, p.y_offset * scale};
        result.xAdvance += glyph.xAdvance; result.yAdvance += glyph.yAdvance;
        result.missingGlyphs |= glyph.id == 0; result.glyphs.push_back(glyph);
    }
    return result;
}
}
