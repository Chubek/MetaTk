#pragma once
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>
namespace metatex::fonts {
struct FontEntry { std::string id; std::filesystem::path path; unsigned faceIndex = 0; };
// UTF-8, line-based format: metatex-fonts 1, then "id" "path" faceIndex.
// Blank lines and full-line # comments are allowed. Paths are manifest-relative.
struct FontManifest {
    std::vector<FontEntry> entries;
    static FontManifest parse(std::string_view text, const std::filesystem::path& base = {});
    static FontManifest read(const std::filesystem::path& path);
    std::string serialize() const;
};
}
