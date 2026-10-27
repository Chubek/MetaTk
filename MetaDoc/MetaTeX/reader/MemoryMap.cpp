#include "MetaTeX-Reader.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <system_error>
#include <cerrno>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
namespace metatex::io {
MappedFileReader::MappedFileReader(const std::filesystem::path& path) {
    int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) throw std::system_error(errno, std::generic_category(), "open mapped file");
    struct stat info{};
    if (::fstat(fd, &info) != 0 || info.st_size < 0 || !S_ISREG(info.st_mode)) {
        int error = errno;
        ::close(fd);
        throw std::system_error(error ? error : EINVAL, std::generic_category(), "stat mapped file");
    }
    length_ = static_cast<std::size_t>(info.st_size);
    if (length_) {
        void* mapping = ::mmap(nullptr, length_, PROT_READ, MAP_PRIVATE, fd, 0);
        if (mapping == MAP_FAILED) {
            int error = errno;
            ::close(fd);
            throw std::system_error(error, std::generic_category(), "mmap input");
        }
        map_ = mapping;
    }
    ::close(fd);
}
MappedFileReader::~MappedFileReader() { if (map_) ::munmap(const_cast<void*>(map_), length_); }
std::size_t MappedFileReader::read(char* destination, std::size_t capacity) {
    auto n = std::min(capacity, length_ - offset_);
    if (n) std::memcpy(destination, static_cast<const char*>(map_) + offset_, n);
    offset_ += n;
    return n;
}
}
