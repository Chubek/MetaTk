#pragma once
#include "../cpp/Support.h"
#include <bit>
#include <limits>

namespace metalib {
namespace msgpack_detail {
using namespace support;
inline Value value(int tag, V args = {}) {
  static const char *names[] = {"MpNil", "MpBool", "MpInt", "MpFloat", "MpString", "MpBinary", "MpArray", "MpMap", "MpExt"};
  return metaml::make_ctor(names[tag], tag, std::move(args));
}
inline void number(std::string &out, std::uint64_t n, unsigned bytes) {
  for (unsigned i = bytes; i; --i) out += static_cast<char>(n >> ((i - 1) * 8));
}
inline void length(std::string &out, std::size_t n, unsigned tag, unsigned bytes) {
  if (n > UINT32_MAX) throw std::runtime_error("MessagePack value too large");
  out += static_cast<char>(tag); number(out, n, bytes);
}
inline void pack(const Value &v, std::string &out, unsigned depth = 0) {
  if (depth > 128) throw std::runtime_error("MessagePack nesting exceeds 128");
  if (is_ctor(v, "MpNil")) out += '\xc0';
  else if (is_ctor(v, "MpBool")) out += arg(v, 0).i ? '\xc3' : '\xc2';
  else if (is_ctor(v, "MpInt")) {
    auto n = arg(v, 0).i;
    if (n >= -32 && n <= 127) out += static_cast<char>(n);
    else { out += '\xd3'; number(out, static_cast<std::uint64_t>(n), 8); }
  } else if (is_ctor(v, "MpFloat")) {
    out += '\xcb'; number(out, std::bit_cast<std::uint64_t>(arg(v, 0).f), 8);
  } else if (is_ctor(v, "MpString") || is_ctor(v, "MpBinary")) {
    const auto &bytes = S(arg(v, 0));
    length(out, bytes.size(), is_ctor(v, "MpString") ? 0xdb : 0xc6, 4); out += bytes;
  } else if (is_ctor(v, "MpArray") || is_ctor(v, "MpMap")) {
    bool map = is_ctor(v, "MpMap");
    auto entries = items(arg(v, 0));
    length(out, entries.size(), map ? 0xdf : 0xdd, 4);
    for (const auto &entry : entries) {
      if (map) { pack(fst(entry), out, depth + 1); pack(snd(entry), out, depth + 1); }
      else pack(entry, out, depth + 1);
    }
  } else if (is_ctor(v, "MpExt")) {
    auto type = arg(v, 0).i;
    if (type < -128 || type > 127) throw std::runtime_error("extension type must fit int8");
    const auto &bytes = S(arg(v, 1));
    length(out, bytes.size(), 0xc9, 4); out += static_cast<char>(type); out += bytes;
  } else throw std::runtime_error("invalid MessagePack value");
}
struct Reader {
  std::string_view bytes;
  std::size_t pos = 0;
  std::uint64_t number(unsigned count) {
    if (count > bytes.size() - pos) throw std::runtime_error("truncated MessagePack value");
    std::uint64_t result = 0;
    while (count--) result = (result << 8) | static_cast<unsigned char>(bytes[pos++]);
    return result;
  }
  std::string text(std::uint64_t n) {
    if (n > bytes.size() - pos) throw std::runtime_error("truncated MessagePack payload");
    std::string result(bytes.substr(pos, n)); pos += n; return result;
  }
  Value collection(std::uint64_t count, bool map, unsigned depth) {
    if (count > (bytes.size() - pos) / (map ? 2 : 1)) throw std::runtime_error("truncated MessagePack collection");
    V values;
    for (std::uint64_t i = 0; i < count; ++i) {
      auto first = read(depth + 1);
      if (map) { auto second = read(depth + 1); values.push_back(pair(first, second)); }
      else values.push_back(first);
    }
    return value(map ? 7 : 6, {list(values)});
  }
  Value read(unsigned depth = 0) {
    if (depth > 128) throw std::runtime_error("MessagePack nesting exceeds 128");
    auto tag = number(1);
    if (tag <= 0x7f) return value(2, {integer(tag)});
    if (tag >= 0xe0) return value(2, {integer(static_cast<std::int64_t>(tag) - 256)});
    if ((tag & 0xe0) == 0xa0) return value(4, {str(text(tag & 31))});
    if ((tag & 0xf0) == 0x90) return collection(tag & 15, false, depth);
    if ((tag & 0xf0) == 0x80) return collection(tag & 15, true, depth);
    switch (tag) {
      case 0xc0: return value(0);
      case 0xc2: case 0xc3: return value(1, {boolean(tag == 0xc3)});
      case 0xca: return value(3, {floating(std::bit_cast<float>(static_cast<std::uint32_t>(number(4))))});
      case 0xcb: return value(3, {floating(std::bit_cast<double>(number(8)))});
      case 0xcc: case 0xcd: case 0xce: case 0xcf: {
        auto n = number(1u << (tag - 0xcc));
        if (n > INT64_MAX) throw std::runtime_error("unsigned integer exceeds MetaML Int");
        return value(2, {integer(n)});
      }
      case 0xd0: case 0xd1: case 0xd2: case 0xd3: {
        unsigned width = 1u << (tag - 0xd0); auto n = number(width);
        if (width < 8 && (n & (std::uint64_t(1) << (width * 8 - 1)))) n |= (~std::uint64_t(0)) << (width * 8);
        return value(2, {integer(std::bit_cast<std::int64_t>(n))});
      }
      case 0xc4: case 0xc5: case 0xc6: return value(5, {str(text(number(1u << (tag - 0xc4))))});
      case 0xd9: case 0xda: case 0xdb: return value(4, {str(text(number(1u << (tag - 0xd9))))});
      case 0xdc: case 0xdd: return collection(number(tag == 0xdc ? 2 : 4), false, depth);
      case 0xde: case 0xdf: return collection(number(tag == 0xde ? 2 : 4), true, depth);
      case 0xd4: case 0xd5: case 0xd6: case 0xd7: case 0xd8:
      case 0xc7: case 0xc8: case 0xc9: {
        auto n = tag >= 0xd4 ? (1u << (tag - 0xd4)) : number(1u << (tag - 0xc7));
        auto type = number(1); auto signed_type = type < 128 ? static_cast<int>(type) : static_cast<int>(type) - 256;
        return value(8, {integer(signed_type), str(text(n))});
      }
      default: throw std::runtime_error("reserved MessagePack tag");
    }
  }
};
}
inline void install_MsgPack(metaml::Session &s) {
  using namespace support;
  if (s.find_primitive("MsgPack.pack")) return;
  s.declare("type MsgValue := | MpNil | MpBool Bool | MpInt Int | MpFloat Float | MpString String | MpBinary String | MpArray [MsgValue] | MpMap [(MsgValue, MsgValue)] | MpExt Int String\n", "<MsgPack>");
  prim(s, "MsgPack.pack", "MsgValue -> Result String String", 1, [](C &, V &a) {
    try { std::string out; msgpack_detail::pack(a[0], out); return ok(str(out)); }
    catch (const std::exception &e) { return err(e.what()); }
  });
  prim(s, "MsgPack.unpack", "String -> Result MsgValue String", 1, [](C &, V &a) {
    try {
      msgpack_detail::Reader reader{S(a[0])}; auto result = reader.read();
      if (reader.pos != reader.bytes.size()) return err("trailing MessagePack bytes");
      return ok(result);
    } catch (const std::exception &e) { return err(e.what()); }
  });
}
}
