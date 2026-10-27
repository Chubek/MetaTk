#pragma once
#include <cerrno>
#include <chrono>
#include <climits>
#include <string>
#include <system_error>
#include <poll.h>
#include <unistd.h>
namespace metatex::io::detail {
[[noreturn]] inline void fail(const char* operation) {
    throw std::system_error(errno, std::generic_category(), operation);
}
class FD {
public:
    explicit FD(int fd = -1) : fd_(fd) {}
    ~FD() { if (fd_ >= 0) ::close(fd_); }
    FD(const FD&) = delete;
    FD& operator=(const FD&) = delete;
    int get() const { return fd_; }
    int release() { int result = fd_; fd_ = -1; return result; }
private:
    int fd_;
};
inline void ipcName(const std::string& name) {
    if (name.size() < 2 || name[0] != '/' || name.find('/', 1) != std::string::npos || name.find('\0') != std::string::npos)
        throw std::invalid_argument("POSIX IPC name must be /name with no further slash");
}
using Deadline = std::chrono::steady_clock::time_point;
inline Deadline deadline(std::chrono::milliseconds timeout) {
    if (timeout.count() <= 0 || timeout > std::chrono::hours(24))
        throw std::invalid_argument("timeout must be positive and at most 24 hours");
    return std::chrono::steady_clock::now() + timeout;
}
inline void wait(int fd, short events, Deadline until) {
    for (;;) {
        auto left = until - std::chrono::steady_clock::now();
        if (left <= decltype(left)::zero()) throw std::runtime_error("I/O timed out");
        auto ms = std::chrono::ceil<std::chrono::milliseconds>(left).count();
        pollfd item{fd, events, 0};
        int ready = ::poll(&item, 1, static_cast<int>(ms > INT_MAX ? INT_MAX : ms));
        if (ready > 0) {
            if (item.revents & POLLNVAL) throw std::runtime_error("invalid I/O descriptor");
            return;
        }
        if (ready < 0 && errno != EINTR) fail("poll");
    }
}
inline timespec realtimeDeadline(std::chrono::milliseconds timeout) {
    (void)deadline(timeout);
    auto value = std::chrono::system_clock::now().time_since_epoch() + timeout;
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(value);
    return {static_cast<time_t>(seconds.count()), static_cast<long>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(value - seconds).count())};
}
}
