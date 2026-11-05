#pragma once
// Architecture-only subroutine-threaded block compiler. No MetaML dependency.
#include <cstdint>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#if !defined(METAML_NO_JIT) && defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif !defined(METAML_NO_JIT) && (defined(__unix__) || defined(__APPLE__))
#include <sys/mman.h>
#endif

namespace metaml::native {
enum class Target { Amd64, Riscv64, Aarch64, Unsupported };
inline Target host()
{
#if defined(__arm64e__) || (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
  return Target::Unsupported;
#elif defined(__x86_64__) || defined(_M_X64)
  return Target::Amd64;
#elif defined(__aarch64__) || defined(_M_ARM64)
  return Target::Aarch64;
#elif defined(__riscv) && __riscv_xlen == 64
  return Target::Riscv64;
#else
  return Target::Unsupported;
#endif
}
inline void little(std::string &out, std::uint64_t value, unsigned bytes)
{
  for (unsigned i = 0; i < bytes; ++i) out += static_cast<char>(value >> (i * 8));
}
inline void patch(std::string &out, std::size_t at, std::uint64_t value, unsigned bytes)
{
  for (unsigned i = 0; i < bytes; ++i) out.at(at + i) = static_cast<char>(value >> (i * 8));
}
struct Call { std::uintptr_t function, data; };
// Entry: int(void *context). Every call: int(void *context, const void *data).
// Zero continues; nonzero exits the block, preserving that return value.
inline std::string block(Target target, const std::vector<Call> &calls)
{
  if (calls.empty() || calls.size() > 32) throw std::runtime_error("invalid native block size");
  std::string out;
  std::vector<std::size_t> exits;
  auto word = [&](std::uint32_t n) { little(out, n, 4); };
  auto arm_address = [&](unsigned reg, std::uintptr_t value) {
    for (unsigned i = 0; i < 4; ++i)
      word((i ? 0xf2800000u : 0xd2800000u) | (i << 21) | (((value >> (16 * i)) & 65535) << 5) | reg);
  };
  auto rv_address = [&](unsigned reg, std::uintptr_t value) {
    if (out.size() % 8) word(0x00000013); // naturally aligned ld literal
    word((reg << 7) | 0x17); // auipc rd, 0
    word(0x01003003 | (reg << 15) | (reg << 7)); // ld rd,16(rd)
    word(0x0100006f); word(0x00000013); little(out, value, 8); // skip literal
  };
  if (target == Target::Amd64) {
    out.assign("\xf3\x0f\x1e\xfa\x53", 5); // ENDBR64; push rbx
#if defined(_WIN32)
    out.append("\x48\x83\xec\x20\x48\x89\xcb", 7); // shadow space; rbx=rcx
#else
    out.append("\x48\x89\xfb", 3); // rbx=rdi
#endif
    for (auto call : calls) {
#if defined(_WIN32)
      out.append("\x48\x89\xd9\x48\xba", 5); // rcx=rbx; rdx=data
#else
      out.append("\x48\x89\xdf\x48\xbe", 5); // rdi=rbx; rsi=data
#endif
      little(out, call.data, 8); out.append("\x48\xb8", 2); little(out, call.function, 8);
      out.append("\xff\xd0\x85\xc0\x0f\x85", 6); exits.push_back(out.size()); little(out, 0, 4);
    }
    auto end = out.size();
    for (auto at : exits) patch(out, at, end - at - 4, 4);
#if defined(_WIN32)
    out.append("\x48\x83\xc4\x20", 4);
#endif
    out.append("\x5b\xc3", 2);
  } else if (target == Target::Aarch64) {
    word(0xd503245f); word(0xa9bf7bf3); word(0xaa0003f3); // BTI; save x19/lr; x19=context
    for (auto call : calls) {
      word(0xaa1303e0); arm_address(1, call.data); arm_address(16, call.function); word(0xd63f0200);
      exits.push_back(out.size()); word(0);
    }
    auto end = out.size();
    for (auto at : exits) patch(out, at, 0x35000000u | (((end - at) / 4) << 5), 4); // cbnz w0,end
    word(0xa8c17bf3); word(0xd65f03c0);
  } else if (target == Target::Riscv64) {
    word(0xff010113); word(0x00113423); word(0x00813023); word(0x00050413);
    for (auto call : calls) {
      word(0x00040513); rv_address(11, call.data); rv_address(5, call.function); word(0x000280e7);
      exits.push_back(out.size()); word(0);
    }
    auto end = out.size();
    for (auto at : exits) {
      auto n = static_cast<unsigned>(end - at);
      patch(out, at, 0x00051063u | ((n >> 12) << 31) | (((n >> 5) & 63) << 25)
        | (((n >> 1) & 15) << 8) | (((n >> 11) & 1) << 7), 4);
    }
    word(0x00013403); word(0x00813083); word(0x01010113); word(0x00008067);
  } else throw std::runtime_error("unsupported native architecture");
  return out;
}
class Executable {
  void *memory_ = nullptr;
  std::size_t size_ = 0;
public:
  explicit Executable(const std::string &code) : size_(code.size())
  {
    if (!size_) throw std::runtime_error("empty native code");
    if (size_ > SIZE_MAX - 16) throw std::runtime_error("native code size overflow");
    size_ += 16; // readable prefix for Clang's indirect-function sanitizer probe
#if !defined(METAML_NO_JIT) && defined(_WIN32)
    memory_ = VirtualAlloc(nullptr, size_, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!memory_) throw std::runtime_error("JIT allocation failed");
    std::memset(memory_, 0, 16);
    std::memcpy(static_cast<char *>(memory_) + 16, code.data(), code.size());
    DWORD old;
    if (!VirtualProtect(memory_, size_, PAGE_EXECUTE_READ, &old)
        || !FlushInstructionCache(GetCurrentProcess(), memory_, size_)) {
      VirtualFree(memory_, 0, MEM_RELEASE); memory_ = nullptr;
      throw std::runtime_error("JIT executable memory is unavailable");
    }
#elif !defined(METAML_NO_JIT) && (defined(__unix__) || defined(__APPLE__))
    memory_ = mmap(nullptr, size_, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (memory_ == MAP_FAILED) { memory_ = nullptr; throw std::runtime_error("JIT allocation failed"); }
    std::memset(memory_, 0, 16);
    std::memcpy(static_cast<char *>(memory_) + 16, code.data(), code.size());
    if (mprotect(memory_, size_, PROT_READ | PROT_EXEC)) {
      munmap(memory_, size_); memory_ = nullptr;
      throw std::runtime_error("JIT executable memory is unavailable");
    }
    __builtin___clear_cache(static_cast<char *>(memory_), static_cast<char *>(memory_) + size_);
#else
    throw std::runtime_error("JIT executable memory is unavailable");
#endif
  }
  Executable(const Executable &) = delete;
  Executable &operator=(const Executable &) = delete;
  ~Executable()
  {
    if (!memory_) return;
#if !defined(METAML_NO_JIT) && defined(_WIN32)
    VirtualFree(memory_, 0, MEM_RELEASE);
#elif !defined(METAML_NO_JIT) && (defined(__unix__) || defined(__APPLE__))
    munmap(memory_, size_);
#endif
  }
  template<class F> F entry() const { return reinterpret_cast<F>(static_cast<char *>(memory_) + 16); }
};
}
