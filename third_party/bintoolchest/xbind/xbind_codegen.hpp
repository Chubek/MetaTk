#ifndef XBIND_CODEGEN_HPP
#define XBIND_CODEGEN_HPP

#include "xbind_qamrpp.hpp"

namespace xbind::codegen {
// Deterministic, target-neutral spec IR. This does not imply header parsing.
std::string dump_ir(const qamrpp::Manifest& manifest);
// Compile the returned C++20 translation unit to verify exact C signatures.
std::string emit_check(const qamrpp::Manifest& manifest);
// Lua 5.4 scalar/string/explicit opaque-pointer bindings via DomLuaBind.
std::string emit_lua(const qamrpp::Manifest& manifest, const std::string& root);
}

#endif
