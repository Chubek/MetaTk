#pragma once
namespace metalib {
namespace http_detail {
inline constexpr std::size_t max_body = 16 * 1024 * 1024, max_headers = 65536;
inline bool token (std::string_view s)
{
  if (s.empty ()) return false;
  for (unsigned char ch : s)
    if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9')
          || std::string_view ("!#$%&'*+-.^_`|~").find (ch) != std::string_view::npos)) return false;
  return true;
}
inline bool header_value (std::string_view s)
{
  for (unsigned char ch : s) if ((ch < 32 && ch != '\t') || ch == 127) return false;
  return true;
}
struct Api
{
  void *library = nullptr;
  int (*global_init) (long) = nullptr;
  void *(*init) () = nullptr;
  int (*setopt) (void *, int, ...) = nullptr;
  int (*perform) (void *) = nullptr;
  int (*getinfo) (void *, int, ...) = nullptr;
  void (*cleanup) (void *) = nullptr;
  void *(*append) (void *, const char *) = nullptr;
  void (*free_list) (void *) = nullptr;
  const char *(*error) (int) = nullptr;
  bool initialized = false;
  Api ()
  {
    for (auto name : { "libcurl.so.4", "libcurl.so", "libcurl.dylib" })
      if ((library = ::dlopen (name, RTLD_NOW | RTLD_LOCAL))) break;
    if (!library) return;
#define METAML_CURL_LOAD(field, symbol) field = reinterpret_cast<decltype (field)> (::dlsym (library, symbol))
    METAML_CURL_LOAD (global_init, "curl_global_init"); METAML_CURL_LOAD (init, "curl_easy_init");
    METAML_CURL_LOAD (setopt, "curl_easy_setopt"); METAML_CURL_LOAD (perform, "curl_easy_perform");
    METAML_CURL_LOAD (getinfo, "curl_easy_getinfo"); METAML_CURL_LOAD (cleanup, "curl_easy_cleanup");
    METAML_CURL_LOAD (append, "curl_slist_append"); METAML_CURL_LOAD (free_list, "curl_slist_free_all");
    METAML_CURL_LOAD (error, "curl_easy_strerror");
#undef METAML_CURL_LOAD
    initialized = global_init && init && setopt && perform && getinfo && cleanup && append && free_list && error
                  && global_init (3L) == 0;
  }
};
inline Api &api () { static Api a; return a; }
struct Response
{
  std::string body;
  std::vector<std::pair<std::string, std::string>> headers;
  std::size_t header_bytes = 0;
  bool exceeded = false;
  metaml::Interp *interp = nullptr;
};
inline std::size_t body_callback (char *bytes, std::size_t size, std::size_t count, void *user) noexcept
{
  auto &r = *static_cast<Response *> (user);
  if (size && count > max_body / size) { r.exceeded = true; return 0; }
  auto n = size * count;
  if (n > max_body - r.body.size ()) { r.exceeded = true; return 0; }
  try { r.body.append (bytes, n); return n; } catch (...) { return 0; }
}
inline std::size_t header_callback (char *bytes, std::size_t size, std::size_t count, void *user) noexcept
{
  auto &r = *static_cast<Response *> (user);
  if (size && count > max_headers / size) { r.exceeded = true; return 0; }
  auto n = size * count;
  if (n > max_headers - r.header_bytes) { r.exceeded = true; return 0; }
  r.header_bytes += n;
  try {
    std::string line (bytes, n);
    if (line.compare (0, 5, "HTTP/") == 0) r.headers.clear ();
    else if (auto p = line.find (':'); p != std::string::npos)
      r.headers.push_back ({ metaml::detail::ascii_map (line.substr (0, p), false), metaml::detail::trim_ws (line.substr (p + 1)) });
    return n;
  } catch (...) { return 0; }
}
inline int progress_callback (void *user, std::int64_t, std::int64_t, std::int64_t, std::int64_t) noexcept
{
  auto *interp = static_cast<Response *> (user)->interp;
  try { return interp && interp->cancelled && interp->cancelled () ? 1 : 0; } catch (...) { return 1; }
}
inline metaml::Value request (support::C &c, const std::string &method, const std::string &url,
                             const metaml::Value &headers, const std::string &body, std::int64_t timeout)
{
  using namespace support;
  if (!api ().initialized) return err ("HTTP.request: libcurl is unavailable");
  if (!token (method)) return err ("HTTP.request: invalid method");
  if ((url.compare (0, 7, "http://") != 0 && url.compare (0, 8, "https://") != 0)
      || !header_value (url) || url.find ('\t') != std::string::npos || url.find (' ') != std::string::npos)
    return err ("HTTP.request: expected an http:// or https:// URL");
  if (timeout <= 0 || timeout > 300000) return err ("HTTP.request: timeout must be in 1..300000 milliseconds");
  if (body.size () > max_body) return err ("HTTP.request: body exceeds 16 MiB");
  auto &a = api ();
  void *easy = a.init ();
  if (!easy) return err ("HTTP.request: cannot allocate a client");
  struct Guard { Api &a; void *easy; void *headers = nullptr; ~Guard () { if (headers) a.free_list (headers); a.cleanup (easy); } } guard{ a, easy };
  std::size_t header_bytes = 0;
  for (const auto &h : items (headers)) {
    auto name = S (fst (h)), value = S (snd (h));
    if (!token (name) || !header_value (value)) return err ("HTTP.request: invalid header");
    auto line = name + ": " + value;
    if (line.size () + 2 > max_headers - header_bytes) return err ("HTTP.request: headers exceed 64 KiB");
    header_bytes += line.size () + 2;
    void *next = a.append (guard.headers, line.c_str ());
    if (!next) return err ("HTTP.request: cannot allocate headers");
    guard.headers = next;
  }
  Response response; response.interp = &c.interp;
  // Stable public libcurl option constants; each vararg uses its documented C type.
  bool configured = a.setopt (easy, 10002, url.c_str ()) == 0 && a.setopt (easy, 10036, method.c_str ()) == 0
    && a.setopt (easy, 155, static_cast<long> (timeout)) == 0 && a.setopt (easy, 156, static_cast<long> (timeout)) == 0
    && a.setopt (easy, 99, 1L) == 0 && a.setopt (easy, 52, 0L) == 0 && a.setopt (easy, 181, 3L) == 0
    && a.setopt (easy, 10177, "*") == 0 && a.setopt (easy, 20011, &body_callback) == 0
    && a.setopt (easy, 10001, &response) == 0 && a.setopt (easy, 20079, &header_callback) == 0
    && a.setopt (easy, 10029, &response) == 0 && a.setopt (easy, 20219, &progress_callback) == 0
    && a.setopt (easy, 10057, &response) == 0 && a.setopt (easy, 43, 0L) == 0;
  if (guard.headers) configured = configured && a.setopt (easy, 10023, guard.headers) == 0;
  if (!body.empty () || method == "POST" || method == "PUT" || method == "PATCH")
    configured = configured && a.setopt (easy, 30120, static_cast<std::int64_t> (body.size ())) == 0
                 && a.setopt (easy, 10015, body.data ()) == 0;
  if (method == "HEAD") configured = configured && a.setopt (easy, 44, 1L) == 0;
  if (!configured) return err ("HTTP.request: cannot configure the client");
  int status = a.perform (easy);
  c.interp.check_cancelled (c.span);
  if (response.exceeded) return err ("HTTP.request: response exceeds size limit");
  if (status) return err (std::string ("HTTP.request: ") + a.error (status));
  long code = 0; a.getinfo (easy, 0x200002, &code);
  V hs; for (const auto &h : response.headers) hs.push_back (pair (str (h.first), str (h.second)));
  return ok (record ({ { "status", integer (code) }, { "headers", list (hs) }, { "body", str (response.body) } }));
}
}
inline void install_HTTP (metaml::Session &s)
{
  using namespace support;
  if (s.find_primitive ("HTTP.request")) return;
  s.declare ("type HttpResponse = { status : Int, headers : [(String, String)], body : String }\n", "<metalib HTTP>");
  prim (s, "HTTP.available", "Unit -> Bool", 1, [] (C &, V &) { return boolean (http_detail::api ().initialized); });
  prim (s, "HTTP.request", "String -> String -> [(String, String)] -> String -> Int -> Result HttpResponse String", 5,
    [] (C &c, V &a) { return http_detail::request (c, S (a[0]), S (a[1]), a[2], S (a[3]), a[4].i); });
  prim (s, "HTTP.get", "String -> Result HttpResponse String", 1,
    [] (C &c, V &a) { return http_detail::request (c, "GET", S (a[0]), list ({}), "", 10000); });
  prim (s, "HTTP.post", "String -> String -> Result HttpResponse String", 2,
    [] (C &c, V &a) { return http_detail::request (c, "POST", S (a[0]), list ({}), S (a[1]), 10000); });
}
}
