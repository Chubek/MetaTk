#ifndef METACHAIN_QSCRIPT_RUNTIME_CONFIG_H
#define METACHAIN_QSCRIPT_RUNTIME_CONFIG_H
#include "qscript/include/QScript-Generated.h"
#include "qscript/src/Types.h"
#include <cstring>

namespace qscript::detail {
// Shared table validation for the C99 bridge and the statically linked native
// runtime. Neither imports bytecode or compiles a QScript program at runtime.
inline Result<Unit> runtime_unit(const qs_runtime_config& config, const Options& options) {
    const auto fail = [](std::string message) -> Result<Unit> {
        return {std::nullopt, {{metachain::Stage::Verify, std::move(message), {}, 0, 0, 0}}};
    };
    if ((!config.strings && config.string_count) || (!config.types && config.type_count) ||
        (!config.grammars && config.grammar_count)) return fail("null runtime table");
    if (config.string_count > options.toolchain.il.max_instructions ||
        config.type_count > options.toolchain.il.max_instructions ||
        config.grammar_count > options.toolchain.il.max_instructions) return fail("runtime table limit exceeded");
    Unit unit;
    std::size_t bytes = 0;
    const auto copy = [&](const char* text, std::size_t length) -> std::optional<std::string> {
        if ((!text && length) || length > options.toolchain.il.max_source_bytes - bytes) return std::nullopt;
        bytes += length;
        return std::string(text ? text : "", length);
    };
    for (std::size_t i = 0; i < config.string_count; ++i) {
        auto text = copy(config.strings[i].data, config.strings[i].length);
        if (!text) return fail("null runtime text or runtime source size limit exceeded");
        unit.strings.push_back(std::move(*text));
    }
    for (std::size_t i = 0; i < config.type_count; ++i) {
        if (!config.types[i] || !parse_type(config.types[i])) return fail("invalid runtime type descriptor");
        auto text = copy(config.types[i], std::strlen(config.types[i]));
        if (!text) return fail("runtime source size limit exceeded");
        unit.types.push_back(std::move(*text));
    }
    for (std::size_t i = 0; i < config.grammar_count; ++i) {
        const auto& g = config.grammars[i];
        if (!g.name) return fail("null runtime grammar name");
        auto name = copy(g.name, std::strlen(g.name)), source = copy(g.source, g.length);
        if (!name || !source) return fail("null runtime text or runtime source size limit exceeded");
        unit.grammars.push_back({std::move(*name), std::move(*source), {}});
    }
    return {std::move(unit), {}};
}
} // namespace qscript::detail
#endif
