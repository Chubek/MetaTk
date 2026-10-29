#pragma once
#include "doxtk/DoxTk.hpp"

// IR serialization interfaces.
// This header is intentionally dependency-free. Add stable public declarations
// here and keep implementations in the matching src/DoxTkSerializer.cpp.
namespace doxtk {
// plaintext emits UTF-8; json-dump and xml-dump are inspection envelopes, not
// readers for arbitrary JSON/XML. Metadata cannot be silently dropped.
[[nodiscard]] Result<std::vector<std::byte>> serialize_document(
    const Document& document, std::string_view format);

struct SerializerModuleInfo {
    static constexpr std::string_view description = "IR serialization interfaces.";
};
} // namespace doxtk
