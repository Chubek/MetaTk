#include "FIFO.hpp"
#include "../utils/Posix.hpp"
#include <fcntl.h>
#include <sys/stat.h>
namespace metatex::io {
namespace {
int openFIFO(const std::filesystem::path& path) {
    detail::FD fd(::open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW));
    if (fd.get() < 0) detail::fail("open FIFO reader");
    struct stat info{};
    if (::fstat(fd.get(), &info) < 0) detail::fail("stat FIFO");
    if (!S_ISFIFO(info.st_mode)) throw std::invalid_argument("input is not a FIFO");
    return fd.release();
}
}
FIFOReader::FIFOReader(const std::filesystem::path& path) : DescriptorReader(openFIFO(path), true) {}
}
