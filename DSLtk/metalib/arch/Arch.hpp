#pragma once
#include "../cpp/Support.h"

namespace metalib::arch {
using Target = metaml::native::Target;
inline Target host()
{
  return metaml::native::host();
}
using metaml::native::Call;
using metaml::native::block;
inline void little(std::string &out, std::uint64_t n, unsigned bytes)
{
  for (unsigned i = 0; i < bytes; ++i) out += static_cast<char>(n >> (i * 8));
}
// ABI-transparent tail jump: the caller's arguments and return address survive.
inline std::string jump(Target target, std::uint64_t address)
{
  std::string out;
  switch (target) {
  case Target::Amd64:
    out.assign("\xf3\x0f\x1e\xfa\x48\xb8", 6); // ENDBR64, movabs rax
    little(out, address, 8); out.append("\xff\xe0", 2); break;
  case Target::Aarch64:
    little(out, 0xd503245f, 4); // BTI c
    little(out, 0x58000070, 4); // ldr x16, literal at +12
    little(out, 0xd61f0200, 4); // br x16
    little(out, 0xd503201f, 4); // nop, aligned literal
    little(out, address, 8); break;
  case Target::Riscv64:
    little(out, 0x00000297, 4); // auipc t0, 0
    little(out, 0x0102b283, 4); // ld t0, 16(t0)
    little(out, 0x00028067, 4); // jr t0
    little(out, 0x00000013, 4); // nop
    little(out, address, 8); break;
  default: throw std::runtime_error("unsupported architecture");
  }
  return out;
}
inline std::string return_int(Target target, std::uint64_t value)
{
  std::string out;
  switch (target) {
  case Target::Amd64:
    out.assign("\xf3\x0f\x1e\xfa\x48\xb8", 6);
    little(out, value, 8); out += '\xc3'; break;
  case Target::Aarch64:
    little(out, 0xd503245f, 4);
    for (unsigned i = 0; i < 4; ++i)
      little(out, (i ? 0xf2800000 : 0xd2800000) | (i << 21)
                  | (((value >> (16 * i)) & 0xffff) << 5), 4);
    little(out, 0xd65f03c0, 4); break;
  case Target::Riscv64:
    little(out, 0x00000517, 4); // auipc a0, 0
    little(out, 0x01053503, 4); // ld a0, 16(a0)
    little(out, 0x00008067, 4); // ret
    little(out, 0x00000013, 4);
    little(out, value, 8); break;
  default: throw std::runtime_error("unsupported architecture");
  }
  return out;
}
}
namespace metalib {
inline void install_Arch(metaml::Session &s)
{
  using namespace support;
  if (s.find_primitive("Amd64.native")) return;
  for (auto [name, target] : std::vector<std::pair<std::string, arch::Target>>{
      {"Amd64", arch::Target::Amd64}, {"Riscv64", arch::Target::Riscv64},
      {"Aarch64", arch::Target::Aarch64}}) {
    prim(s, name + ".native", "Unit -> Bool", 1,
      [target](C &, V &) { return boolean(target == arch::host()); });
    prim(s, name + ".return_int", "Int -> String", 1,
      [target](C &, V &a) { return str(arch::return_int(target, static_cast<std::uint64_t>(a[0].i))); });
  }
}
}
