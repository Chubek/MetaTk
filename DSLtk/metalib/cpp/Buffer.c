// metalib/cpp/Buffer.c — implementation of Buffer.h.
// This is C++ (the metalib host modules are header-only); it is #included by
// Buffer.h and must not be compiled on its own. Mirror: metalib/d/Buffer.d.
#pragma once

namespace metalib
{


inline void
install_Buffer (metaml::Session &s)
{
  using namespace support;
  using H = Handle<dsl::BinaryStream>;
  s.declare ("type Buffer\n", "<metalib Buffer>");
  prim (s, "Buffer.create", "Unit -> Buffer", 1,
        [] (C &, V &) { return wrap_handle (std::make_shared<H> ("Buffer")); });
  auto append = [] (H &h, const std::string &text)
    {
      std::lock_guard<std::mutex> g (h.mutex);
      for (char c : text)
        h.data.bytes.push_back (static_cast<std::byte> (static_cast<unsigned char> (c)));
    };
  prim (s, "Buffer.add", "String -> Buffer -> Unit", 2, [append] (C &c, V &a) {
    append (handle<dsl::BinaryStream> (a[1], "Buffer", c), S (a[0]));
    return unit ();
  });
  prim (s, "Buffer.add_line", "String -> Buffer -> Unit", 2, [append] (C &c, V &a) {
    append (handle<dsl::BinaryStream> (a[1], "Buffer", c), S (a[0]) + "\n");
    return unit ();
  });
  prim (s, "Buffer.add_byte", "Int -> Buffer -> Unit", 2, [] (C &c, V &a) {
    auto &h = handle<dsl::BinaryStream> (a[1], "Buffer", c);
    std::lock_guard<std::mutex> g (h.mutex);
    h.data.write_integer (static_cast<std::uint8_t> (a[0].i & 255));
    return unit ();
  });
  struct W
  {
    const char *name;
    int bytes;
    bool little;
  };
  static const W ws[] = { { "Buffer.add_u16le", 2, true },
                          { "Buffer.add_u32le", 4, true },
                          { "Buffer.add_u64le", 8, true },
                          { "Buffer.add_u32be", 4, false } };
  for (auto &w : ws)
    {
      int n = w.bytes;
      auto order = w.little ? dsl::ByteOrder::Little : dsl::ByteOrder::Big;
      prim (s, w.name, "Int -> Buffer -> Unit", 2, [n, order] (C &c, V &a) {
        auto &h = handle<dsl::BinaryStream> (a[1], "Buffer", c);
        std::lock_guard<std::mutex> g (h.mutex);
        auto v = static_cast<std::uint64_t> (a[0].i);
        if (n == 2)
          h.data.write_integer (static_cast<std::uint16_t> (v), order);
        else if (n == 4)
          h.data.write_integer (static_cast<std::uint32_t> (v), order);
        else
          h.data.write_integer (v, order);
        return unit ();
      });
    }
  prim (s, "Buffer.contents", "Buffer -> String", 1, [] (C &c, V &a) {
    auto &h = handle<dsl::BinaryStream> (a[0], "Buffer", c);
    std::lock_guard<std::mutex> g (h.mutex);
    std::string out;
    for (auto b : h.data.bytes)
      out += static_cast<char> (std::to_integer<unsigned char> (b));
    return str (out);
  });
  prim (s, "Buffer.length", "Buffer -> Int", 1, [] (C &c, V &a) {
    auto &h = handle<dsl::BinaryStream> (a[0], "Buffer", c);
    std::lock_guard<std::mutex> g (h.mutex);
    return integer (static_cast<std::int64_t> (h.data.bytes.size ()));
  });
  prim (s, "Buffer.byte_at", "Int -> Buffer -> Option Int", 2, [] (C &c, V &a) {
    auto &h = handle<dsl::BinaryStream> (a[1], "Buffer", c);
    std::lock_guard<std::mutex> g (h.mutex);
    if (a[0].i < 0 || a[0].i >= static_cast<std::int64_t> (h.data.bytes.size ()))
      return none ();
    return some (integer (std::to_integer<unsigned char> (h.data.bytes[static_cast<std::size_t> (a[0].i)])));
  });
  prim (s, "Buffer.clear", "Buffer -> Unit", 1, [] (C &c, V &a) {
    auto &h = handle<dsl::BinaryStream> (a[0], "Buffer", c);
    std::lock_guard<std::mutex> g (h.mutex);
    h.data.bytes.clear ();
    h.data.pos = 0;
    return unit ();
  });
  prim (s, "Buffer.of_string", "String -> Buffer", 1, [append] (C &, V &a) {
    auto h = std::make_shared<H> ("Buffer");
    append (*h, S (a[0]));
    return wrap_handle (h);
  });
  prim (s, "Buffer.truncate", "Int -> Buffer -> Unit", 2, [] (C &c, V &a) {
    auto &h = handle<dsl::BinaryStream> (a[1], "Buffer", c);
    std::lock_guard<std::mutex> g (h.mutex);
    if (a[0].i < 0 || static_cast<std::uint64_t> (a[0].i) > h.data.bytes.size ())
      c.error ("M0404", "`Buffer.truncate` length is outside the buffer");
    h.data.bytes.resize (static_cast<std::size_t> (a[0].i));
    h.data.pos = std::min (h.data.pos, h.data.bytes.size ());
    return unit ();
  });
  prim (s, "Buffer.sub", "Int -> Int -> Buffer -> Option String", 3, [] (C &c, V &a) {
    auto &h = handle<dsl::BinaryStream> (a[2], "Buffer", c);
    std::lock_guard<std::mutex> g (h.mutex);
    auto n = h.data.bytes.size ();
    if (a[0].i < 0 || a[1].i < 0 || static_cast<std::uint64_t> (a[0].i) > n
        || static_cast<std::uint64_t> (a[1].i) > n - static_cast<std::size_t> (a[0].i))
      return none ();
    std::string out;
    for (std::size_t i = 0; i < static_cast<std::size_t> (a[1].i); ++i)
      out += static_cast<char> (std::to_integer<unsigned char> (h.data.bytes[a[0].i + i]));
    return some (str (out));
  });
  prim (s, "Buffer.hex", "Buffer -> String", 1, [] (C &c, V &a) {
    auto &h = handle<dsl::BinaryStream> (a[0], "Buffer", c);
    std::lock_guard<std::mutex> g (h.mutex);
    static const char *d = "0123456789abcdef";
    std::string out;
    for (auto b : h.data.bytes)
      {
        auto x = std::to_integer<unsigned char> (b);
        out += d[x >> 4];
        out += d[x & 15];
      }
    return str (out);
  });
}

} // namespace metalib
