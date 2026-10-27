#include "Socket.hpp"
#include "../utils/Memory.hpp"
#include "../utils/Posix.hpp"
#include <cstring>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/un.h>
namespace metatex::io {
namespace {
int connectAddress(int family, const sockaddr* address, socklen_t length, detail::Deadline until) {
    detail::FD fd(::socket(family, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0));
    if (fd.get() < 0) detail::fail("create socket");
    if (::connect(fd.get(), address, length) < 0) {
        if (errno != EINPROGRESS) detail::fail("connect socket");
        detail::wait(fd.get(), POLLOUT, until);
        int error = 0; socklen_t size = sizeof(error);
        if (::getsockopt(fd.get(), SOL_SOCKET, SO_ERROR, &error, &size) < 0) detail::fail("socket error");
        if (error) throw std::system_error(error, std::generic_category(), "connect socket");
    }
    return fd.release();
}
}
int connectTCP(const std::string& host, const std::string& service, std::chrono::milliseconds timeout) {
    (void)detail::deadline(timeout);
    if (host.empty() || service.empty() || host.find('\0') != std::string::npos || service.find('\0') != std::string::npos)
        throw std::invalid_argument("invalid TCP address");
    addrinfo hints{}; hints.ai_family = AF_UNSPEC; hints.ai_socktype = SOCK_STREAM;
    addrinfo* addresses = nullptr;
    int error = ::getaddrinfo(host.c_str(), service.c_str(), &hints, &addresses);
    if (error) throw std::runtime_error(std::string("resolve TCP address: ") + ::gai_strerror(error));
    util::ScopeExit cleanup([&] { ::freeaddrinfo(addresses); });
    auto until = detail::deadline(timeout); std::exception_ptr last;
    for (auto* a = addresses; a; a = a->ai_next) {
        try { return connectAddress(a->ai_family, a->ai_addr, a->ai_addrlen, until); }
        catch (const std::system_error&) { last = std::current_exception(); }
    }
    if (last) std::rethrow_exception(last);
    throw std::runtime_error("TCP address has no stream endpoints");
}
int connectUnix(const std::filesystem::path& path, std::chrono::milliseconds timeout) {
    sockaddr_un address{}; address.sun_family = AF_UNIX; auto name = path.string();
    if (name.empty() || name.size() >= sizeof(address.sun_path) || name.find('\0') != std::string::npos)
        throw std::invalid_argument("invalid Unix socket path");
    std::memcpy(address.sun_path, name.c_str(), name.size() + 1);
    return connectAddress(AF_UNIX, reinterpret_cast<sockaddr*>(&address), sizeof(address), detail::deadline(timeout));
}
SocketReader::SocketReader(int descriptor, bool own, std::chrono::milliseconds timeout)
    : fd_(descriptor), own_(own), timeout_(timeout) {
    if (fd_ < 0) throw std::invalid_argument("invalid socket descriptor");
    (void)detail::deadline(timeout);
}
SocketReader::~SocketReader() { if (own_) ::close(fd_); }
std::size_t SocketReader::read(char* destination, std::size_t capacity) {
    if (!capacity) return 0;
    if (!destination) throw std::invalid_argument("null read destination");
    auto until = detail::deadline(timeout_);
    for (;;) {
        detail::wait(fd_, POLLIN, until);
        auto n = ::recv(fd_, destination, capacity, MSG_DONTWAIT);
        if (n >= 0) return static_cast<std::size_t>(n);
        if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) detail::fail("receive socket");
    }
}
}
