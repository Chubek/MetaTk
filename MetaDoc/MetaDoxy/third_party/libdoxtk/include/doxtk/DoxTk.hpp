#pragma once
#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace doxtk {

inline constexpr std::string_view version = "0.1.0";

enum class ErrorCode {
    ok = 0, invalid_argument, io_error, parse_error, unsupported_format,
    unsupported_conversion, serialization_error, dependency_missing, internal_error
};

struct Error {
    ErrorCode code{ErrorCode::ok};
    std::string message;
    std::string context;
    [[nodiscard]] explicit operator bool() const noexcept { return code != ErrorCode::ok; }
};

template<class T>
struct Result {
    std::optional<T> value;
    Error error;
    std::vector<std::string> diagnostics;
    [[nodiscard]] bool has_value() const noexcept { return value.has_value() && !error; }
    [[nodiscard]] explicit operator bool() const noexcept { return has_value(); }
    static Result success(T v, std::vector<std::string> diagnostics = {}) {
        return {std::move(v), {}, std::move(diagnostics)};
    }
    static Result failure(ErrorCode c, std::string m, std::string ctx = {}) {
        return {std::nullopt, {c, std::move(m), std::move(ctx)}, {}};
    }
};

// Structured IRs may carry an immutable tree alongside their decoded source.
struct Node;

struct Document {
    std::string ir;
    std::string media_type;
    std::string encoding{"utf-8"};
    std::vector<std::byte> data;
    std::unordered_map<std::string, std::string> metadata;
    std::shared_ptr<const Node> tree;
};

struct ConvertOptions {
    bool strict{true};
    bool allow_lossy{false};
    std::string source_format;
    std::string target_format;
    std::string source_encoding{"utf-8"};
};

class Translator {
public:
    virtual ~Translator() = default;
    [[nodiscard]] virtual Result<Document> translate(
        const Document& input, std::string_view target_ir,
        const ConvertOptions& options = {}) const = 0;
};

[[nodiscard]] std::string_view error_code_name(ErrorCode code) noexcept;
[[nodiscard]] Result<std::vector<std::byte>> read_file(const std::filesystem::path& path);
[[nodiscard]] Error write_file(const std::filesystem::path& path, std::span<const std::byte> bytes);

} // namespace doxtk
