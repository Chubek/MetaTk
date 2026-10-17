#pragma once

// Optional C++ convenience adapter. The bridge itself is C99 + QScript;
// including this header does not include QScript's C++ compiler facade.
#include "MLVM-Bridge.h"
#include "metachain/MetaIL.hpp"
#include "metachain/Outcome.hpp"
#include <exception>
#include <memory>

namespace mlvm::metair {
struct ModuleDeleter { void operator()(mlvm_module* m) const noexcept { mlvm_module_destroy(m); } };
using Module = std::unique_ptr<mlvm_module, ModuleDeleter>;
namespace detail {
struct Buffer {
    mlvm_buffer value{};
    ~Buffer() { mlvm_buffer_destroy(&value); }
};
inline metachain::Diagnostic error(std::string message) {
    return {metachain::Stage::Lower, std::move(message), {}, 0, 0, 0};
}
}
inline metachain::Outcome<Module> import_program(mlvm_context* context, const metail::Program& program) {
    try {
        metail::Limits limits;
        limits.max_instructions = MLVM_BRIDGE_MAX_INSTRUCTIONS;
        const auto diagnostics = metail::verify(program, limits);
        if (!diagnostics.empty()) {
            const auto& d = diagnostics.front();
            return {std::nullopt, {{metachain::Stage::Verify, d.message, d.source.file, d.source.line, d.source.column, d.instruction}}};
        }
        const auto bytes = metail::encode(program);
        mlvm_module* module = nullptr;
        if (mlvm_import_mcil(context, bytes.data(), bytes.size(), &module) != MLVM_OK)
            return {std::nullopt, {detail::error(mlvm_error(context))}};
        return {Module(module), {}};
    } catch (const std::exception& e) {
        return {std::nullopt, {detail::error(e.what())}};
    } catch (...) {
        return {std::nullopt, {detail::error("MetaIR bridge import failed")}};
    }
}
inline metachain::Outcome<metail::Program> export_program(mlvm_context* context, const mlvm_module* module) {
    try {
        detail::Buffer bytes;
        if (mlvm_export_mcil(context, module, &bytes.value) != MLVM_OK)
            return {std::nullopt, {detail::error(mlvm_error(context))}};
        auto decoded = metail::decode(std::span<const std::uint8_t>(bytes.value.data, bytes.value.size));
        if (!decoded) return {std::nullopt, {detail::error(decoded.diagnostics.front().message)}};
        return {std::move(decoded.program), {}};
    } catch (const std::exception& e) {
        return {std::nullopt, {detail::error(e.what())}};
    } catch (...) {
        return {std::nullopt, {detail::error("MetaIR bridge export failed")}};
    }
}
} // namespace mlvm::metair
