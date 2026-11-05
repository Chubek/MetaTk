#pragma once
#include "../ast/Absyn.hpp"

namespace metalib {
namespace system_detail {
inline bool available(const std::string &name)
{
#if defined(_WIN32)
  return name == "Win32";
#elif defined(__APPLE__)
  return name == "Posix" || name == "MacOS";
#elif defined(__unix__)
  return name == "Posix";
#else
  return false;
#endif
}
inline void no_nul(const std::string &s)
{
  if (s.find('\0') != std::string::npos) throw std::runtime_error("argument contains NUL");
}
}
inline void install_System(metaml::Session &s)
{
  using namespace support;
  if (s.find_primitive("Posix.available")) return;
  for (std::string name : {"Posix", "Win32", "MacOS"}) {
    bool supported = system_detail::available(name);
    prim(s, name + ".available", "Unit -> Bool", 1,
      [supported](C &, V &) { return boolean(supported); });
    prim(s, name + ".cwd", "Unit -> Result String String", 1,
      [supported](C &, V &) {
        if (!supported) return err("OS is unavailable on this host");
        std::error_code ec;
        auto p = std::filesystem::current_path(ec);
        return ec ? err(ec.message()) : ok(str(p.string()));
      });
    prim(s, name + ".getenv", "String -> Result (Option String) String", 1,
      [supported](C &, V &a) {
        if (!supported) return err("OS is unavailable on this host");
        auto &key = S(a[0]);
        if (key.empty() || key.find('=') != std::string::npos || key.find('\0') != std::string::npos)
          return err("invalid environment variable name");
        auto value = std::getenv(key.c_str());
        return ok(value ? some(str(value)) : none());
      });
    prim(s, name + ".read_file", "String -> Result String String", 1,
      [supported](C &, V &a) {
        if (!supported) return err("OS is unavailable on this host");
        try { return ok(str(absyn_detail::read_file(S(a[0])))); }
        catch (const std::exception &e) { return err(e.what()); }
      });
    prim(s, name + ".write_file", "String -> String -> Result Unit String", 2,
      [supported](C &, V &a) {
        if (!supported) return err("OS is unavailable on this host");
        try {
          system_detail::no_nul(S(a[0]));
          std::ofstream f(S(a[0]), std::ios::binary | std::ios::trunc);
          if (!f) return err("cannot open file: " + S(a[0]));
          f.write(S(a[1]).data(), static_cast<std::streamsize>(S(a[1]).size()));
          f.close();
          return f ? ok(unit()) : err("cannot write file: " + S(a[0]));
        } catch (const std::exception &e) { return err(e.what()); }
      });
  }
}
}
