#pragma once
#include "../arch/Arch.hpp"

namespace metalib::objfile {
struct Info { std::string format, arch; unsigned bits = 0, sections = 0; };
inline void range(std::string_view s, std::uint64_t offset, std::uint64_t count)
{
  if (offset > s.size() || count > s.size() - offset)
    throw std::runtime_error("truncated object file");
}
inline std::uint64_t get(std::string_view s, std::size_t offset, unsigned count, bool le = true)
{
  range(s, offset, count);
  std::uint64_t n = 0;
  for (unsigned i = 0; i < count; ++i)
    n |= std::uint64_t(static_cast<unsigned char>(s[offset + i])) << (8 * (le ? i : count - i - 1));
  return n;
}
inline void put(std::string &s, std::size_t offset, std::uint64_t n, unsigned count)
{
  range(s, offset, count);
  for (unsigned i = 0; i < count; ++i) s[offset + i] = static_cast<char>(n >> (8 * i));
}
inline std::string machine(unsigned n, const std::string &format)
{
  if (format == "elf") {
    if (n == 62) return "amd64";
    if (n == 183) return "aarch64";
    if (n == 243) return "riscv64";
  } else if (format == "macho") {
    if (n == 0x01000007) return "amd64";
    if (n == 0x0100000c) return "aarch64";
  } else {
    if (n == 0x8664) return "amd64";
    if (n == 0xaa64) return "aarch64";
    if (n == 0x5064) return "riscv64";
  }
  return "unknown";
}
inline Info inspect(const std::string &format, std::string_view s)
{
  Info out{format, "unknown", 64, 0};
  if (format == "elf") {
    range(s, 0, 16);
    if (s.substr(0, 4) != std::string_view("\x7f" "ELF", 4)) throw std::runtime_error("not ELF");
    unsigned cls = get(s, 4, 1), endian = get(s, 5, 1);
    if ((cls != 1 && cls != 2) || (endian != 1 && endian != 2) || get(s, 6, 1) != 1)
      throw std::runtime_error("invalid ELF identification");
    bool wide = cls == 2, le = endian == 1;
    unsigned header = wide ? 64 : 52, shsize = wide ? 64 : 40;
    range(s, 0, header);
    if (get(s, 20, 4, le) != 1 || get(s, wide ? 52 : 40, 2, le) != header)
      throw std::runtime_error("invalid ELF header");
    out.bits = wide ? 64 : 32;
    out.arch = machine(get(s, 18, 2, le), format);
    auto offset = get(s, wide ? 40 : 32, wide ? 8 : 4, le);
    auto size = get(s, wide ? 58 : 46, 2, le);
    out.sections = get(s, wide ? 60 : 48, 2, le);
    if (!out.sections && offset) throw std::runtime_error("extended ELF section counts are unsupported");
    if (out.sections && (size != shsize || !offset)) throw std::runtime_error("invalid ELF section table");
    range(s, offset, std::uint64_t(size) * out.sections);
    for (unsigned i = 0; i < out.sections; ++i) {
      auto at = offset + i * size;
      if (get(s, at + 4, 4, le) != 8) // SHT_NOBITS has no file payload
        range(s, get(s, at + (wide ? 24 : 16), wide ? 8 : 4, le),
                 get(s, at + (wide ? 32 : 20), wide ? 8 : 4, le));
    }
  } else if (format == "coff" || format == "pe") {
    std::uint64_t at = 0;
    if (format == "pe") {
      range(s, 0, 64);
      if (s.substr(0, 2) != "MZ") throw std::runtime_error("not PE");
      at = get(s, 60, 4);
      if (at < 64) throw std::runtime_error("invalid PE offset");
      range(s, at, 24);
      if (s.substr(at, 4) != std::string_view("PE\0\0", 4)) throw std::runtime_error("invalid PE signature");
      at += 4;
    }
    range(s, at, 20);
    out.arch = machine(get(s, at, 2), format);
    if (out.arch == "unknown") throw std::runtime_error("unsupported COFF machine");
    out.sections = get(s, at + 2, 2);
    auto opt = get(s, at + 16, 2);
    if (format == "coff" && opt) throw std::runtime_error("COFF object has optional header");
    range(s, at + 20, opt);
    if (format == "pe") {
      auto magic = get(s, at + 20, 2);
      unsigned minimum = magic == 0x20b ? 112 : magic == 0x10b ? 96 : 0;
      if (!minimum || opt < minimum) throw std::runtime_error("invalid PE optional header");
      out.bits = magic == 0x20b ? 64 : 32;
      auto dirs = get(s, at + 20 + minimum - 4, 4);
      if (dirs > (opt - minimum) / 8) throw std::runtime_error("truncated PE directories");
    }
    auto table = at + 20 + opt;
    range(s, table, std::uint64_t(out.sections) * 40);
    for (unsigned i = 0; i < out.sections; ++i) {
      auto sh = table + i * 40;
      auto size = get(s, sh + 16, 4);
      if (size) range(s, get(s, sh + 20, 4), size);
    }
  } else if (format == "macho") {
    auto magic = get(s, 0, 4);
    bool le = magic == 0xfeedfacf || magic == 0xfeedface;
    bool wide = magic == 0xfeedfacf || magic == 0xcffaedfe;
    if (!le && magic != 0xcffaedfe && magic != 0xcefaedfe) throw std::runtime_error("not thin Mach-O");
    out.bits = wide ? 64 : 32;
    range(s, 0, wide ? 32 : 28);
    out.arch = machine(get(s, 4, 4, le), format);
    auto count = get(s, 16, 4, le), bytes = get(s, 20, 4, le);
    std::uint64_t at = wide ? 32 : 28, end = at + bytes;
    range(s, at, bytes);
    if (count > bytes / 8) throw std::runtime_error("invalid Mach-O command count");
    for (std::uint64_t i = 0; i < count; ++i) {
      if (at > end || end - at < 8) throw std::runtime_error("truncated Mach-O command");
      auto cmd = get(s, at, 4, le), size = get(s, at + 4, 4, le);
      if (size < 8 || size > end - at || size % (wide ? 8 : 4)) throw std::runtime_error("invalid Mach-O command size");
      if (cmd == 0x19 || cmd == 1) {
        bool seg64 = cmd == 0x19;
        unsigned base = seg64 ? 72 : 56, secsize = seg64 ? 80 : 68;
        if (size < base) throw std::runtime_error("truncated Mach-O segment");
        auto n = get(s, at + (seg64 ? 64 : 48), 4, le);
        if (n > (size - base) / secsize) throw std::runtime_error("truncated Mach-O sections");
        out.sections += static_cast<unsigned>(n);
        for (unsigned j = 0; j < n; ++j) {
          auto sh = at + base + j * secsize;
          auto kind = get(s, sh + (seg64 ? 64 : 56), 4, le) & 255;
          if (kind != 1 && kind != 12 && kind != 18)
            range(s, get(s, sh + (seg64 ? 48 : 40), 4, le), get(s, sh + (seg64 ? 40 : 36), seg64 ? 8 : 4, le));
        }
      }
      at += size;
    }
    if (at != end) throw std::runtime_error("Mach-O command size mismatch");
  } else throw std::runtime_error("unknown object format");
  return out;
}

// Minimal single-text-section images. No symbols, relocations, imports or linker.
// ELF/COFF/Mach-O are relocatable; PE is a PE32+ image with entry RVA 0x1000.
inline std::string of_code(const std::string &format, const std::string &target, const std::string &code)
{
  unsigned elf = target == "amd64" ? 62 : target == "aarch64" ? 183 : target == "riscv64" ? 243 : 0;
  unsigned coff = target == "amd64" ? 0x8664 : target == "aarch64" ? 0xaa64 : target == "riscv64" ? 0x5064 : 0;
  if (!elf) throw std::runtime_error("unknown architecture");
  if (code.size() > 0x3fffffff) throw std::runtime_error("code is too large");
  std::string out;
  if (format == "elf") {
    std::size_t table = (64 + code.size() + 7) & ~std::size_t(7);
    out.assign(table + 192 + 17, '\0');
    out.replace(0, 7, std::string("\x7f" "ELF\x02\x01\x01", 7));
    put(out, 16, 1, 2); put(out, 18, elf, 2); put(out, 20, 1, 4);
    put(out, 40, table, 8); put(out, 52, 64, 2); put(out, 58, 64, 2);
    put(out, 60, 3, 2); put(out, 62, 2, 2);
    out.replace(64, code.size(), code);
    auto text = table + 64, names = table + 128;
    put(out, text, 1, 4); put(out, text + 4, 1, 4); put(out, text + 8, 6, 8);
    put(out, text + 24, 64, 8); put(out, text + 32, code.size(), 8); put(out, text + 48, 16, 8);
    put(out, names, 7, 4); put(out, names + 4, 3, 4); put(out, names + 24, table + 192, 8);
    put(out, names + 32, 17, 8); put(out, names + 48, 1, 8);
    out.replace(table + 192, 17, std::string("\0.text\0.shstrtab\0", 17));
  } else if (format == "coff") {
    out.assign(60, '\0'); put(out, 0, coff, 2); put(out, 2, 1, 2);
    out.replace(20, 5, ".text"); put(out, 36, code.size(), 4); put(out, 40, 60, 4);
    put(out, 56, 0x60500020, 4); out += code;
  } else if (format == "pe") {
    if (code.empty()) throw std::runtime_error("PE entry code cannot be empty");
    auto raw = (code.size() + 511) & ~std::size_t(511);
    out.assign(512 + raw, '\0'); out.replace(0, 2, "MZ"); put(out, 60, 64, 4);
    out.replace(64, 4, std::string("PE\0\0", 4)); put(out, 68, coff, 2); put(out, 70, 1, 2);
    put(out, 84, 240, 2); put(out, 86, 0x22, 2);
    unsigned op = 88; put(out, op, 0x20b, 2); put(out, op + 4, raw, 4);
    put(out, op + 16, 4096, 4); put(out, op + 20, 4096, 4);
    put(out, op + 24, 0x140000000, 8); put(out, op + 32, 4096, 4); put(out, op + 36, 512, 4);
    put(out, op + 40, 6, 2); put(out, op + 48, 6, 2);
    put(out, op + 56, (4096 + code.size() + 4095) & ~std::size_t(4095), 4);
    put(out, op + 60, 512, 4); put(out, op + 68, 3, 2);
    put(out, op + 72, 1048576, 8); put(out, op + 80, 4096, 8);
    put(out, op + 88, 1048576, 8); put(out, op + 96, 4096, 8); put(out, op + 108, 16, 4);
    unsigned sh = op + 240; out.replace(sh, 5, ".text");
    put(out, sh + 8, code.size(), 4); put(out, sh + 12, 4096, 4);
    put(out, sh + 16, raw, 4); put(out, sh + 20, 512, 4); put(out, sh + 36, 0x60000020, 4);
    out.replace(512, code.size(), code);
  } else if (format == "macho") {
    if (target == "riscv64") throw std::runtime_error("Mach-O has no supported riscv64 ABI");
    out.assign(184, '\0'); put(out, 0, 0xfeedfacf, 4);
    put(out, 4, target == "amd64" ? 0x01000007 : 0x0100000c, 4);
    put(out, 8, target == "amd64" ? 3 : 0, 4); put(out, 12, 1, 4);
    put(out, 16, 1, 4); put(out, 20, 152, 4);
    put(out, 32, 0x19, 4); put(out, 36, 152, 4);
    put(out, 64, code.size(), 8); put(out, 72, 184, 8); put(out, 80, code.size(), 8);
    put(out, 88, 7, 4); put(out, 92, 5, 4); put(out, 96, 1, 4);
    out.replace(104, 6, "__text"); out.replace(120, 6, "__TEXT");
    put(out, 144, code.size(), 8); put(out, 152, 184, 4); put(out, 156, 2, 4);
    put(out, 168, 0x80000400, 4); out += code;
  } else throw std::runtime_error("unknown object format");
  return out;
}
}
namespace metalib {
inline void install_Objfile(metaml::Session &s)
{
  using namespace support;
  if (s.find_primitive("ELF.inspect")) return;
  const std::string info = "{format : String, arch : String, bits : Int, sections : Int}";
  for (auto [name, format] : std::vector<std::pair<std::string, std::string>>{
      {"ELF", "elf"}, {"PE", "pe"}, {"COFF", "coff"}, {"MachO", "macho"}}) {
    prim(s, name + ".inspect", "String -> Result (" + info + ") String", 1,
      [format](C &, V &a) {
        try {
          auto i = objfile::inspect(format, S(a[0]));
          return ok(record({{"format", str(i.format)}, {"arch", str(i.arch)},
                            {"bits", integer(i.bits)}, {"sections", integer(i.sections)}}));
        } catch (const std::exception &e) { return err(e.what()); }
      });
    prim(s, name + ".matches", "String -> Bool", 1, [format](C &, V &a) {
      try { objfile::inspect(format, S(a[0])); return boolean(true); }
      catch (const std::exception &) { return boolean(false); }
    });
    prim(s, name + ".of_code", "String -> String -> Result String String", 2,
      [format](C &, V &a) {
        try { return ok(str(objfile::of_code(format, S(a[0]), S(a[1])))); }
        catch (const std::exception &e) { return err(e.what()); }
      });
  }
}
}
