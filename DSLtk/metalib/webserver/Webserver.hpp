#pragma once
#include "../cpp/Support.h"
#include <httplib.h>
namespace metalib {
namespace webserver_detail {
struct Server {
  httplib::Server server;
  std::thread worker;
  bool started = false;
  ~Server() { server.stop(); if (worker.joinable()) worker.join(); }
};
}
inline void install_Webserver(metaml::Session &s) {
  using namespace support;
  if (s.find_primitive("Webserver.create")) return;
  s.declare("type WebServer\n", "<Webserver>");
  prim(s, "Webserver.create", "Unit -> WebServer", 1, [](C &, V &) {
    return wrap_handle(std::make_shared<Handle<webserver_detail::Server>>("WebServer"));
  });
  prim(s, "Webserver.route", "WebServer -> String -> String -> Int -> String -> String -> Result Unit String", 6, [](C &c, V &a) {
    auto &h = handle<webserver_detail::Server>(a[0], "WebServer", c);
    std::lock_guard lock(h.mutex);
    if (h.data.started) return err("register routes before starting the server");
    if (a[3].i < 100 || a[3].i > 599) return err("invalid HTTP status");
    auto content_type = S(a[4]);
    if (content_type.find_first_of("\r\n") != std::string::npos || content_type.find('\0') != std::string::npos) return err("invalid content type");
    auto callback = [status = static_cast<int>(a[3].i), content_type, body = S(a[5])](const httplib::Request &, httplib::Response &response) {
      response.status = status; response.set_content(body, content_type);
    };
    try {
      auto &server = h.data.server; const auto &path = S(a[2]); const auto &method = S(a[1]);
      if (method == "GET") server.Get(path, callback);
      else if (method == "POST") server.Post(path, callback);
      else if (method == "PUT") server.Put(path, callback);
      else if (method == "DELETE") server.Delete(path, callback);
      else if (method == "PATCH") server.Patch(path, callback);
      else if (method == "OPTIONS") server.Options(path, callback);
      else return err("unsupported HTTP method");
    } catch (const std::exception &e) { return err(e.what()); }
    return ok(unit());
  });
  prim(s, "Webserver.start", "WebServer -> String -> Int -> Result Int String", 3, [](C &c, V &a) {
    auto &h = handle<webserver_detail::Server>(a[0], "WebServer", c);
    std::lock_guard lock(h.mutex);
    if (h.data.started) return err("server already started");
    if (a[2].i < 0 || a[2].i > 65535 || S(a[1]).find('\0') != std::string::npos) return err("invalid bind address or port");
    int port = static_cast<int>(a[2].i);
    if (port == 0) port = h.data.server.bind_to_any_port(S(a[1]));
    else if (!h.data.server.bind_to_port(S(a[1]), port)) return err("cannot bind server");
    if (port < 0) return err("cannot bind server");
    h.data.started = true;
    h.data.worker = std::thread([&server = h.data.server] { server.listen_after_bind(); });
    h.data.server.wait_until_ready();
    return ok(integer(port));
  });
  prim(s, "Webserver.stop", "WebServer -> Unit", 1, [](C &c, V &a) {
    auto &h = handle<webserver_detail::Server>(a[0], "WebServer", c);
    std::lock_guard lock(h.mutex);
    h.data.server.stop();
    if (h.data.worker.joinable()) h.data.worker.join();
    h.data.started = false;
    return unit();
  });
}
}
