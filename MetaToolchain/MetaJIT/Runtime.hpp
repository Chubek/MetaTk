#ifndef METAJIT_RUNTIME_HPP
#define METAJIT_RUNTIME_HPP

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace metajit {
struct NativeFrame;
namespace detail {
template<class T> struct Signature { static constexpr bool supported = false; };
template<class R, class... A> struct Signature<R (*)(A...)> {
    static constexpr bool supported = std::is_same_v<R, std::int64_t> &&
        (std::is_same_v<A, std::int64_t> && ...) && sizeof...(A) <= 6;
    static constexpr std::size_t arity = sizeof...(A);
};
template<> struct Signature<std::int64_t (*)(NativeFrame *)> {
    static constexpr bool supported = true;
    static constexpr std::size_t arity = 1;
};
}

struct Entry {
    std::string name;
    std::size_t offset;
    std::size_t arity;
};

/** Move-only owner of RX memory. Pointers borrowed through get() remain valid
 * until destruction or move-assignment of the owner. Concurrent calls through
 * those pointers are supported; lifetime changes must be synchronized.
 * Execution is supported on Linux x86-64 using the System V ABI. */
class Executable {
public:
    Executable() = default;
    ~Executable();
    Executable(const Executable &) = delete;
    Executable &operator=(const Executable &) = delete;
    Executable(Executable &&other) noexcept;
    Executable &operator=(Executable &&other) noexcept;

    /** Signature must be an int64_t function pointer with up to six int64_t
     * arguments, or the internal NativeFrame transition ABI from OSR.hpp.
     * Throws for an absent entry, wrong arity, or empty owner. */
    template<class Signature> Signature get(std::string_view name) const {
        static_assert(detail::Signature<Signature>::supported,
                      "MetaJIT requires an int64 signature or NativeFrame transition");
        auto address = lookup(name, detail::Signature<Signature>::arity);
        Signature function{};
        static_assert(sizeof function == sizeof address);
        std::memcpy(&function, &address, sizeof function);
        return function;
    }
    std::size_t size() const noexcept { return size_; }

private:
    friend class CodeBuffer;
    Executable(void *memory, std::size_t size, std::vector<Entry> entries) noexcept;
    const void *lookup(std::string_view name, std::size_t arity) const;
    void release() noexcept;
    void *memory_ = nullptr;
    std::size_t size_ = 0;
    std::vector<Entry> entries_;
};

/** Writable staging buffer used by generated C++. This is a trusted machine
 * code interface, not a verifier. No allocation is simultaneously writable and
 * executable. Methods throw on invalid metadata, allocation or OS failure.
 * Separate instances may be used concurrently. */
class CodeBuffer {
public:
    static constexpr std::size_t max_size = 64 * 1024 * 1024;
    void append(std::initializer_list<std::uint8_t> bytes);
    void append(const std::vector<std::uint8_t> &bytes);
    void patch64(std::size_t offset, std::uint64_t value);
    void entry(std::string name, std::size_t offset, std::size_t arity);
    Executable finalize() const;
private:
    std::vector<std::uint8_t> bytes_;
    std::vector<Entry> entries_;
};
} // namespace metajit
#endif
