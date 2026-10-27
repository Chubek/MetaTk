#pragma once
#include "FontLoader.hpp"
#include "FontManifest.hpp"
#include <map>
namespace metatex::fonts {
class FontCatalogue {
public:
    // Transactional import: a bad face or duplicate ID leaves the catalogue intact.
    void import(const FontManifest& manifest, FontLoader& loader);
    void add(std::string id, std::shared_ptr<const Font> font);
    std::shared_ptr<const Font> find(std::string_view id) const;
    std::shared_ptr<const Font> match(std::string_view family, bool bold = false, bool italic = false) const;
    std::vector<std::string> ids() const;
private:
    std::map<std::string, std::shared_ptr<const Font>, std::less<>> fonts_;
};
}
