#include "MSGQueue.hpp"
#include "../utils/Posix.hpp"
#include <algorithm>
#include <cstring>
#include <fcntl.h>
namespace metatex::io {
MSGQueueReader::MSGQueueReader(const std::string& name, std::chrono::milliseconds timeout) : timeout_(timeout) {
    detail::ipcName(name); (void)detail::deadline(timeout);
    queue_ = ::mq_open(name.c_str(), O_RDONLY | O_CLOEXEC);
    if (queue_ == static_cast<mqd_t>(-1)) detail::fail("open message queue reader");
    try {
        mq_attr attr{};
        if (::mq_getattr(queue_, &attr) < 0) detail::fail("message queue attributes");
        if (attr.mq_msgsize <= 0 || attr.mq_msgsize > 16 * 1024 * 1024)
            throw std::length_error("message queue message size exceeds limit");
        message_.resize(static_cast<std::size_t>(attr.mq_msgsize));
    } catch (...) { ::mq_close(queue_); throw; }
}
MSGQueueReader::~MSGQueueReader() { ::mq_close(queue_); }
std::size_t MSGQueueReader::read(char* destination, std::size_t capacity) {
    if (!capacity || eof_) return 0;
    if (!destination) throw std::invalid_argument("null read destination");
    if (offset_ == size_) {
        auto until = detail::realtimeDeadline(timeout_); ssize_t n;
        do { n = ::mq_timedreceive(queue_, message_.data(), message_.size(), nullptr, &until); }
        while (n < 0 && errno == EINTR);
        if (n < 0) detail::fail("receive message queue");
        size_ = static_cast<std::size_t>(n); offset_ = 0;
        if (!size_) { eof_ = true; return 0; }
    }
    auto n = std::min(capacity, size_ - offset_);
    std::memcpy(destination, message_.data() + offset_, n); offset_ += n; return n;
}
}
