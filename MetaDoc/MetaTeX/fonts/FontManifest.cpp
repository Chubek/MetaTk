#include "FontManifest.hpp"
#include "MetaTeX-Reader.h"
#include <iomanip>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <unordered_set>
namespace metatex::fonts {
namespace {
void valid(const FontEntry& entry) {
    if (entry.id.empty() || entry.path.empty() || entry.id.find_first_of("\r\n\0", 0, 3) != std::string::npos ||
        entry.path.string().find_first_of("\r\n\0", 0, 3) != std::string::npos || entry.faceIndex > 65535)
        throw std::invalid_argument("invalid font manifest entry");
}
}
FontManifest FontManifest::parse(std::string_view text, const std::filesystem::path& base) {
    if (text.size() > 4 * 1024 * 1024) throw std::length_error("font manifest exceeds limit");
    std::istringstream input{std::string(text)}; input.imbue(std::locale::classic());
    std::string line; bool header = false; FontManifest manifest; std::unordered_set<std::string> ids;
    std::size_t number = 0;
    while (std::getline(input, line)) {
        ++number; std::istringstream record(line); record.imbue(std::locale::classic()); record >> std::ws;
        if (record.eof() || record.peek() == '#') continue;
        if (!header) {
            std::string magic; int version = 0;
            if (!(record >> magic >> version) || magic != "metatex-fonts" || version != 1)
                throw std::runtime_error("invalid font manifest header");
            header = true;
        } else {
            FontEntry entry; std::string path;
            if (!(record >> std::quoted(entry.id) >> std::quoted(path) >> entry.faceIndex))
                throw std::runtime_error("invalid font manifest line " + std::to_string(number));
            entry.path = path; valid(entry);
            if (!ids.insert(entry.id).second) throw std::runtime_error("duplicate font id: " + entry.id);
            if (entry.path.is_relative()) entry.path = base / entry.path;
            manifest.entries.push_back(std::move(entry));
        }
        record >> std::ws;
        if (!record.eof()) throw std::runtime_error("trailing font manifest data on line " + std::to_string(number));
    }
    if (!header) throw std::runtime_error("missing font manifest header");
    return manifest;
}
FontManifest FontManifest::read(const std::filesystem::path& path) {
    io::FileReader reader(path); return parse(reader.readAll(4 * 1024 * 1024), std::filesystem::absolute(path).parent_path());
}
std::string FontManifest::serialize() const {
    std::ostringstream out; out.imbue(std::locale::classic()); out << "metatex-fonts 1\n";
    std::unordered_set<std::string> ids;
    for (const auto& entry : entries) {
        valid(entry);
        if (!ids.insert(entry.id).second) throw std::invalid_argument("duplicate font id: " + entry.id);
        out << std::quoted(entry.id) << ' ' << std::quoted(entry.path.string()) << ' ' << entry.faceIndex << '\n';
    }
    return out.str();
}
}
