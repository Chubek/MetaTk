#pragma once
#include "../cpp/Support.h"
#include <nng/nng.h>
#include <nng/protocol/pair0/pair.h>
#include <nng/protocol/reqrep0/req.h>
#include <nng/protocol/reqrep0/rep.h>
#include <nng/protocol/pubsub0/pub.h>
#include <nng/protocol/pubsub0/sub.h>
namespace metalib {
namespace nng_detail {
struct Socket {
  nng_socket socket{};
  bool open = false;
  ~Socket() { if (open) nng_close(socket); }
};
}
inline void install_NNG(metaml::Session &s) {
  using namespace support;
  if (s.find_primitive("NNG.create")) return;
  s.declare("type NngSocket\n", "<NNG>");
  prim(s, "NNG.create", "String -> Result NngSocket String", 1, [](C &, V &a) {
    auto h = std::make_shared<Handle<nng_detail::Socket>>("NngSocket");
    auto &p = h->data.socket;
    const auto &protocol = S(a[0]);
    int rc;
    if (protocol == "pair") rc = nng_pair0_open(&p);
    else if (protocol == "req") rc = nng_req0_open(&p);
    else if (protocol == "rep") rc = nng_rep0_open(&p);
    else if (protocol == "pub") rc = nng_pub0_open(&p);
    else if (protocol == "sub") rc = nng_sub0_open(&p);
    else return err("unknown NNG protocol");
    if (rc) return err(nng_strerror(rc));
    h->data.open = true;
    nng_socket_set_ms(p, NNG_OPT_SENDTIMEO, 5000);
    nng_socket_set_ms(p, NNG_OPT_RECVTIMEO, 5000);
    return ok(wrap_handle(h));
  });
  for (bool listen : {false, true})
    prim(s, listen ? "NNG.listen" : "NNG.dial", "NngSocket -> String -> Result Unit String", 2, [listen](C &c, V &a) {
      auto &h = handle<nng_detail::Socket>(a[0], "NngSocket", c);
      std::lock_guard lock(h.mutex);
      if (!h.data.open) return err("socket is closed");
      if (S(a[1]).find('\0') != std::string::npos) return err("invalid endpoint");
      int rc = listen ? nng_listen(h.data.socket, S(a[1]).c_str(), nullptr, 0)
                      : nng_dial(h.data.socket, S(a[1]).c_str(), nullptr, NNG_FLAG_NONBLOCK);
      return rc ? err(nng_strerror(rc)) : ok(unit());
    });
  prim(s, "NNG.subscribe", "NngSocket -> String -> Result Unit String", 2, [](C &c, V &a) {
    auto &h = handle<nng_detail::Socket>(a[0], "NngSocket", c);
    std::lock_guard lock(h.mutex);
    if (!h.data.open) return err("socket is closed");
    int rc = nng_socket_set(h.data.socket, NNG_OPT_SUB_SUBSCRIBE, S(a[1]).data(), S(a[1]).size());
    return rc ? err(nng_strerror(rc)) : ok(unit());
  });
  prim(s, "NNG.send", "NngSocket -> String -> Result Unit String", 2, [](C &c, V &a) {
    auto &h = handle<nng_detail::Socket>(a[0], "NngSocket", c);
    std::lock_guard lock(h.mutex);
    if (!h.data.open) return err("socket is closed");
    int rc = nng_send(h.data.socket, const_cast<char *>(S(a[1]).data()), S(a[1]).size(), 0);
    return rc ? err(nng_strerror(rc)) : ok(unit());
  });
  prim(s, "NNG.recv", "NngSocket -> Result String String", 1, [](C &c, V &a) {
    auto &h = handle<nng_detail::Socket>(a[0], "NngSocket", c);
    std::lock_guard lock(h.mutex);
    if (!h.data.open) return err("socket is closed");
    void *data = nullptr; std::size_t size = 0;
    int rc = nng_recv(h.data.socket, &data, &size, NNG_FLAG_ALLOC);
    if (rc) return err(nng_strerror(rc));
    struct Guard { void *p; std::size_t n; ~Guard() { nng_free(p, n); } } guard{data, size};
    return ok(str(std::string(static_cast<char *>(data), size)));
  });
  prim(s, "NNG.close", "NngSocket -> Unit", 1, [](C &c, V &a) {
    auto &h = handle<nng_detail::Socket>(a[0], "NngSocket", c);
    std::lock_guard lock(h.mutex);
    if (h.data.open) { nng_close(h.data.socket); h.data.open = false; }
    return unit();
  });
}
}
