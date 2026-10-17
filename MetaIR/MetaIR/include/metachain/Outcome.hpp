#pragma once

// Shared diagnostics without the collective assembler/JIT/compiler facade.
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace metachain {
enum class Stage { Parse, Verify, Optimize, Execute, Assemble, Link, Lower, Rewrite, IO };
struct Diagnostic {
    Stage stage = Stage::Parse;
    std::string message, file;
    std::size_t line = 0, column = 0, offset = 0;
};
template<class T> struct Outcome {
    std::optional<T> value;
    std::vector<Diagnostic> diagnostics;
    explicit operator bool() const noexcept { return value.has_value() && diagnostics.empty(); }
    T& operator*() { return value.value(); }
    const T& operator*() const { return value.value(); }
    T* operator->() { return &value.value(); }
    const T* operator->() const { return &value.value(); }
};
} // namespace metachain
