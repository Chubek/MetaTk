#include "MetaTeX-Reader.h"
#include <algorithm>
#include <limits>
#include <stdexcept>
namespace metatex::io {
std::size_t StreamReader::read(char* destination, std::size_t capacity) {
    if (!capacity) return 0;
    auto n = std::min(capacity, static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max()));
    stream_.read(destination, static_cast<std::streamsize>(n));
    const auto count = stream_.gcount();
    if (stream_.bad()) throw std::runtime_error("stream reader failed");
    return static_cast<std::size_t>(count);
}
}
