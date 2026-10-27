#include "MetaTeX-Sink.h"
#include <cerrno>
#include <stdexcept>
#include <system_error>
#include <unistd.h>
namespace metatex::io {
FileSink::FileSink(const std::filesystem::path& path) : stream_(path, std::ios::binary | std::ios::trunc) {
    if (!stream_) throw std::runtime_error("cannot open output: " + path.string());
}
void FileSink::write(std::string_view bytes) {
    stream_.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!stream_) throw std::runtime_error("output file write failed");
}
void FileSink::flush() {
    stream_.flush();
    if (!stream_) throw std::runtime_error("output file flush failed");
}
DescriptorSink::DescriptorSink(int descriptor, bool own) : fd_(descriptor), own_(own) {
    if (fd_ < 0) throw std::invalid_argument("negative output descriptor");
}
DescriptorSink::~DescriptorSink() { if (own_) ::close(fd_); }
void DescriptorSink::write(std::string_view bytes) {
    const char* at = bytes.data();
    auto remaining = bytes.size();
    while (remaining) {
        auto written = ::write(fd_, at, remaining);
        if (written < 0 && errno == EINTR) continue;
        if (written <= 0) throw std::system_error(written == 0 ? EIO : errno, std::generic_category(), "write descriptor");
        remaining -= static_cast<std::size_t>(written);
        at += written;
    }
}
void DescriptorSink::flush() { /* unbuffered descriptor writes */ }
}
