#pragma once
#include <optional>
#include <string>
#include <utility>

namespace weighstone::support {
enum class Decision { proven, disproven, unknown };
struct Verdict {
    Decision decision = Decision::unknown;
    std::string reason;
    constexpr bool is_proven() const noexcept { return decision == Decision::proven; }
    constexpr bool is_disproven() const noexcept { return decision == Decision::disproven; }
};
template<class T> struct Result {
    std::optional<T> value;
    std::string error;
    explicit operator bool() const noexcept { return value.has_value(); }
    static Result success(T v) { return {std::move(v), {}}; }
    static Result failure(std::string why) { return {std::nullopt, std::move(why)}; }
};
}
