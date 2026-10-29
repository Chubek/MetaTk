#include "doxtk/DoxTk.hpp"

namespace doxtk {
std::string_view error_code_name(ErrorCode code) noexcept {
    switch (code) {
    case ErrorCode::ok: return "ok";
    case ErrorCode::invalid_argument: return "invalid_argument";
    case ErrorCode::io_error: return "io_error";
    case ErrorCode::parse_error: return "parse_error";
    case ErrorCode::unsupported_format: return "unsupported_format";
    case ErrorCode::unsupported_conversion: return "unsupported_conversion";
    case ErrorCode::serialization_error: return "serialization_error";
    case ErrorCode::dependency_missing: return "dependency_missing";
    case ErrorCode::internal_error: return "internal_error";
    }
    return "unknown";
}

} // namespace doxtk
