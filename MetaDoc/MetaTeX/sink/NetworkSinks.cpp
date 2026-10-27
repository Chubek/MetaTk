#include "NetworkSinks.hpp"
#include "../utils/Posix.hpp"
#include <sys/socket.h>
namespace metatex::io {
SocketSink::SocketSink(int descriptor, bool own, std::chrono::milliseconds timeout)
    : fd_(descriptor), own_(own), timeout_(timeout) {
    if (fd_ < 0) throw std::invalid_argument("invalid socket descriptor");
    (void)detail::deadline(timeout);
}
SocketSink::~SocketSink() { if (own_) ::close(fd_); }
void SocketSink::write(std::string_view bytes) {
    if (finished_) throw std::logic_error("socket stream already finished");
    auto until = detail::deadline(timeout_);
    while (!bytes.empty()) {
        detail::wait(fd_, POLLOUT, until);
        auto n = ::send(fd_, bytes.data(), bytes.size(), MSG_DONTWAIT | MSG_NOSIGNAL);
        if (n > 0) bytes.remove_prefix(static_cast<std::size_t>(n));
        else if (n == 0) throw std::runtime_error("zero-length socket write");
        else if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) detail::fail("send socket");
    }
}
void SocketSink::finish() {
    if (finished_) return;
    if (::shutdown(fd_, SHUT_WR) < 0) detail::fail("finish socket stream");
    finished_ = true;
}
}
