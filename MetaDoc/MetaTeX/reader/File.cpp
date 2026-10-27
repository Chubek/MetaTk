#include "MetaTeX-Reader.h"
#include <algorithm>
#include <limits>
#include <stdexcept>
namespace metatex::io {
std::string Reader::readAll(std::size_t limit) {
    std::string text;
    char bytes[8192];
    for (;;) {
        const auto available = limit - text.size();
        // Request one additional byte at the limit to distinguish exact EOF.
        const auto count = read(bytes, std::min(sizeof(bytes), available + (available == 0)));
        if (!count) break;
        if (count > available) throw std::runtime_error("input byte limit exceeded");
        text.append(bytes, count);
    }
    return text;
}
FileReader::FileReader(const std::filesystem::path& path) : stream_(path, std::ios::binary) {
    if (!stream_) throw std::runtime_error("cannot open input: " + path.string());
}
std::size_t FileReader::read(char* destination, std::size_t capacity) {
    if (!capacity) return 0;
    auto n = std::min(capacity, static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max()));
    stream_.read(destination, static_cast<std::streamsize>(n));
    auto got = stream_.gcount();
    if (stream_.bad()) throw std::runtime_error("file reader failed");
    return static_cast<std::size_t>(got);
}
std::size_t MemoryReader::read(char* destination, std::size_t capacity) {
    auto n = std::min(capacity, data_.size() - offset_);
    if (n) data_.copy(destination, n, offset_);
    offset_ += n;
    return n;
}
}
