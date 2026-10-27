#include "FontCatalogue.hpp"
#include <stdexcept>
namespace metatex::fonts {
void FontCatalogue::import(const FontManifest& manifest, FontLoader& loader) {
    FontCatalogue pending = *this;
    for (const auto& entry : manifest.entries) pending.add(entry.id, loader.load(entry.path, entry.faceIndex));
    fonts_.swap(pending.fonts_);
}
void FontCatalogue::add(std::string id, std::shared_ptr<const Font> font) {
    if (id.empty() || !font) throw std::invalid_argument("font ID and font must be nonempty");
    if (!fonts_.emplace(std::move(id), std::move(font)).second) throw std::invalid_argument("duplicate font ID");
}
std::shared_ptr<const Font> FontCatalogue::find(std::string_view id) const {
    auto found = fonts_.find(id); return found == fonts_.end() ? nullptr : found->second;
}
std::shared_ptr<const Font> FontCatalogue::match(std::string_view family, bool bold, bool italic) const {
    std::shared_ptr<const Font> best; int bestScore = 3;
    for (const auto& [id, font] : fonts_) {
        if (font->metadata.family != family) continue;
        int score = (font->metadata.bold != bold) + (font->metadata.italic != italic);
        if (score < bestScore) { bestScore = score; best = font; }
    }
    return best;
}
std::vector<std::string> FontCatalogue::ids() const {
    std::vector<std::string> result;
    for (const auto& [id, font] : fonts_) result.push_back(id);
    return result;
}
}
