#include "FontLoader.hpp"
#include "MetaTeX-Reader.h"
namespace metatex::fonts {
std::shared_ptr<const Font> FontLoader::load(const std::filesystem::path& path, unsigned faceIndex) {
    auto canonical = std::filesystem::canonical(path);
    const auto key = canonical.string() + '\0' + std::to_string(faceIndex);
    std::lock_guard lock(mutex_);
    if (auto found = cache_.find(key); found != cache_.end())
        if (auto font = found->second.lock()) return font;
    io::FileReader reader(canonical); auto bytes = reader.readAll(limit_);
    auto font = std::make_shared<Font>(); font->path = canonical;
    font->bytes.assign(bytes.begin(), bytes.end());
    font->metadata = inspectFont(font->bytes, faceIndex);
    cache_[key] = font; return font;
}
void FontLoader::clear() { std::lock_guard lock(mutex_); cache_.clear(); }
}
