#include "MetaTeX-Reader.h"
#include <cerrno>
#include <stdexcept>
#include <system_error>
#include <unistd.h>
namespace metatex::io {
DescriptorReader::DescriptorReader(int descriptor, bool own) : fd_(descriptor), own_(own) {
    if (fd_ < 0) throw std::invalid_argument("negative input descriptor");
}
DescriptorReader::~DescriptorReader() { if (own_) ::close(fd_); }
std::size_t DescriptorReader::read(char* destination, std::size_t capacity) {
    if (!capacity) return 0;
    for (;;) {
        auto count = ::read(fd_, destination, capacity);
        if (count >= 0) return static_cast<std::size_t>(count);
        if (errno == EINTR) continue;
        throw std::system_error(errno, std::generic_category(), "read descriptor");
    }
}
}
