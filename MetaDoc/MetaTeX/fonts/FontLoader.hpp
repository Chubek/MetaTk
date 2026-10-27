#pragma once
#include "FontMetadata.hpp"
#include <filesystem>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>
namespace metatex::fonts {
struct Font {
    std::filesystem::path path;
    FontMetadata metadata;
    std::vector<std::uint8_t> bytes;
};
class FontLoader {
public:
    explicit FontLoader(std::size_t maxBytes = 64u * 1024u * 1024u) : limit_(maxBytes) {}
    // Immutable snapshots are cached while referenced. clear() invalidates cache
    // entries when the application changes font files on disk.
    std::shared_ptr<const Font> load(const std::filesystem::path& path, unsigned faceIndex = 0);
    void clear();
private:
    std::size_t limit_;
    std::mutex mutex_;
    std::unordered_map<std::string, std::weak_ptr<const Font>> cache_;
};
}
