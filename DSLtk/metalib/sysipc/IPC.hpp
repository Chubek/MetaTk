#pragma once
#include "../cpp/Support.h"
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <poll.h>
namespace metalib {
namespace ipc_detail {
struct Endpoint { int fd = -1; ~Endpoint() { if (fd >= 0) ::close(fd); } };
}
inline void install_IPC(metaml::Session &s) {
  using namespace support;
  if (s.find_primitive("IPC.pair")) return;
  s.declare("type IpcEndpoint\n", "<IPC>");
  prim(s, "IPC.pair", "Unit -> Result (IpcEndpoint, IpcEndpoint) String", 1, [](C &, V &) {
    auto a = std::make_shared<Handle<ipc_detail::Endpoint>>("IpcEndpoint");
    auto b = std::make_shared<Handle<ipc_detail::Endpoint>>("IpcEndpoint");
    int fds[2];
    if (::socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, fds)) return err(std::strerror(errno));
    a->data.fd = fds[0]; b->data.fd = fds[1];
    return ok(pair(wrap_handle(a), wrap_handle(b)));
  });
  prim(s, "IPC.send", "IpcEndpoint -> String -> Result Unit String", 2, [](C &c, V &a) {
    auto &h = handle<ipc_detail::Endpoint>(a[0], "IpcEndpoint", c);
    std::lock_guard lock(h.mutex);
    if (h.data.fd < 0) return err("endpoint is closed");
    if (S(a[1]).size() > 65536) return err("message exceeds 65536 bytes");
    ssize_t n;
    do {
      n = ::send(h.data.fd, S(a[1]).data(), S(a[1]).size(), MSG_NOSIGNAL | MSG_DONTWAIT);
      if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        pollfd pfd{h.data.fd, POLLOUT, 0};
        int ready = ::poll(&pfd, 1, 1000);
        if (ready > 0) continue;
        if (ready == 0) { errno = EAGAIN; break; }
      }
    } while (n < 0 && errno == EINTR);
    return n < 0 ? err(std::strerror(errno)) : ok(unit());
  });
  prim(s, "IPC.recv", "IpcEndpoint -> Result String String", 1, [](C &c, V &a) {
    auto &h = handle<ipc_detail::Endpoint>(a[0], "IpcEndpoint", c);
    std::lock_guard lock(h.mutex);
    if (h.data.fd < 0) return err("endpoint is closed");
    char bytes[65536]; ssize_t n;
    do { n = ::recv(h.data.fd, bytes, sizeof(bytes), MSG_DONTWAIT); } while (n < 0 && errno == EINTR);
    return n < 0 ? err(std::strerror(errno)) : ok(str(std::string(bytes, n)));
  });
  prim(s, "IPC.close", "IpcEndpoint -> Unit", 1, [](C &c, V &a) {
    auto &h = handle<ipc_detail::Endpoint>(a[0], "IpcEndpoint", c);
    std::lock_guard lock(h.mutex);
    if (h.data.fd >= 0) { ::close(h.data.fd); h.data.fd = -1; }
    return unit();
  });
}
}
