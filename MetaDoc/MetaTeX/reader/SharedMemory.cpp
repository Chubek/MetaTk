#include "SharedMemory.hpp"
#include "../utils/Posix.hpp"
#include <algorithm>
#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
namespace metatex::io {
SharedMemoryReader::SharedMemoryReader(const std::string& name, std::size_t maxBytes) {
    detail::ipcName(name);
    detail::FD fd(::shm_open(name.c_str(), O_RDONLY | O_CLOEXEC, 0));
    if (fd.get() < 0) detail::fail("open shared memory");
    struct stat info{};
    if (::fstat(fd.get(), &info) < 0) detail::fail("stat shared memory");
    if (info.st_size < 0 || static_cast<std::uintmax_t>(info.st_size) > maxBytes)
        throw std::length_error("shared memory input limit exceeded");
    bytes_.resize(static_cast<std::size_t>(info.st_size));
    std::size_t at = 0;
    while (at < bytes_.size()) {
        auto n = ::pread(fd.get(), bytes_.data() + at, bytes_.size() - at, static_cast<off_t>(at));
        if (n < 0) { if (errno == EINTR) continue; detail::fail("read shared memory"); }
        if (n == 0) throw std::runtime_error("shared memory resized during read");
        at += static_cast<std::size_t>(n);
    }
}
std::size_t SharedMemoryReader::read(char* destination, std::size_t capacity) {
    auto n = std::min(capacity, bytes_.size() - offset_);
    if (n && !destination) throw std::invalid_argument("null read destination");
    if (n) std::memcpy(destination, bytes_.data() + offset_, n);
    offset_ += n; return n;
}
}
