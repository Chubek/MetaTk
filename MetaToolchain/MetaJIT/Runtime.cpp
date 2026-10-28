#include "Runtime.hpp"

#include <algorithm>
#include <cerrno>
#include <stdexcept>
#include <system_error>
#include <utility>
#if defined(__linux__) && defined(__x86_64__)
#include <sys/mman.h>
#endif

namespace metajit {
Executable::Executable(void *memory, std::size_t size, std::vector<Entry> entries) noexcept
    : memory_(memory), size_(size), entries_(std::move(entries)) {}
Executable::~Executable() { release(); }
void Executable::release() noexcept {
#if defined(__linux__) && defined(__x86_64__)
    if (memory_) munmap(memory_, size_);
#endif
    memory_ = nullptr;
    size_ = 0;
}
Executable::Executable(Executable &&other) noexcept
    : memory_(std::exchange(other.memory_, nullptr)), size_(std::exchange(other.size_, 0)),
      entries_(std::move(other.entries_)) {}
Executable &Executable::operator=(Executable &&other) noexcept {
    if (this != &other) {
        release();
        memory_ = std::exchange(other.memory_, nullptr);
        size_ = std::exchange(other.size_, 0);
        entries_ = std::move(other.entries_);
    }
    return *this;
}
const void *Executable::lookup(std::string_view name, std::size_t arity) const {
    if (!memory_) throw std::logic_error("MetaJIT: empty executable");
    auto found = std::find_if(entries_.begin(), entries_.end(),
                             [&](const Entry &e) { return e.name == name; });
    if (found == entries_.end()) throw std::invalid_argument("MetaJIT: unknown entry: " + std::string(name));
    if (found->arity != arity) throw std::invalid_argument("MetaJIT: signature arity mismatch: " + found->name);
    return static_cast<const std::uint8_t *>(memory_) + found->offset;
}
void CodeBuffer::append(std::initializer_list<std::uint8_t> bytes) {
    if (bytes.size() > max_size - bytes_.size()) throw std::length_error("MetaJIT: code size limit exceeded");
    bytes_.insert(bytes_.end(), bytes.begin(), bytes.end());
}
void CodeBuffer::append(const std::vector<std::uint8_t> &bytes) {
    if (bytes.size() > max_size - bytes_.size()) throw std::length_error("MetaJIT: code size limit exceeded");
    bytes_.insert(bytes_.end(), bytes.begin(), bytes.end());
}
void CodeBuffer::patch64(std::size_t offset, std::uint64_t value) {
    if (offset > bytes_.size() || bytes_.size() - offset < 8)
        throw std::out_of_range("MetaJIT: immediate patch outside code");
    for (unsigned i = 0; i < 8; ++i) bytes_[offset + i] = static_cast<std::uint8_t>(value >> (8 * i));
}
void CodeBuffer::entry(std::string name, std::size_t offset, std::size_t arity) {
    if (name.empty() || offset >= bytes_.size() || arity > 6)
        throw std::invalid_argument("MetaJIT: invalid entry metadata");
    for (const auto &e : entries_)
        if (e.name == name) throw std::invalid_argument("MetaJIT: duplicate entry: " + name);
    entries_.push_back({std::move(name), offset, arity});
}
Executable CodeBuffer::finalize() const {
    if (bytes_.empty() || entries_.empty()) throw std::logic_error("MetaJIT: no code or entries");
#if defined(__linux__) && defined(__x86_64__)
    auto entries = entries_; // All potentially throwing C++ allocation precedes mmap.
    void *memory = mmap(nullptr, bytes_.size(), PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (memory == MAP_FAILED) throw std::system_error(errno, std::generic_category(), "MetaJIT mmap");
    std::memcpy(memory, bytes_.data(), bytes_.size());
    if (mprotect(memory, bytes_.size(), PROT_READ | PROT_EXEC) != 0) {
        const int error = errno;
        munmap(memory, bytes_.size());
        throw std::system_error(error, std::generic_category(), "MetaJIT mprotect RX");
    }
    return Executable(memory, bytes_.size(), std::move(entries));
#else
    throw std::runtime_error("MetaJIT execution requires Linux x86-64 System V");
#endif
}
} // namespace metajit
