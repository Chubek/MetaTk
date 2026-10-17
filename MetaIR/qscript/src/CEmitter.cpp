// The portable C99 backend for QScript. Programs without built-ins become the
// generic standalone program of metachain::lower_c99. Programs that use
// strings and lists also get the run-time of CRuntime.cpp, with tables that
// describe this unit: the string literals, the type descriptors and the
// numbers of the built-ins. Grammars and matches use the linked C ABI bridge
// to the same C++ grammar engine; generated arithmetic/control flow stays C99.
#include "qscript/QScript.h"
#include "metachain/CSource.hpp"
#include "qscript/src/Runtime.h"
#include "qscript/src/Types.h"
#include <cstdio>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace qscript {
namespace {
using detail::Builtin;
using detail::Type;

// A C string literal holding exactly `s` (embedded NUL bytes included).
void c_string(std::ostringstream& out, std::string_view s) {
    constexpr std::string_view safe = " ._,-:;!#$%&'()*+/<=>[]{}|~^@";
    out << '"';
    for (unsigned char c : s) {
        const bool plain = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                           safe.find(static_cast<char>(c)) != std::string_view::npos;
        if (plain) { out << static_cast<char>(c); continue; }
        char octal[8];
        std::snprintf(octal, sizeof octal, "\\%03o", static_cast<unsigned>(c));
        out << octal;
    }
    out << '"';
}

// The run-time source, followed by the standalone helpers the main program uses.
std::string c_runtime_source_text() {
    std::ostringstream out;
    out << detail::c_runtime_source();
    out << "#ifdef QSCRIPT_STANDALONE\n"
        << "static int qs_rt_stdout_text(const char *text, size_t n, void *user) {\n"
        << "    (void)user;\n"
        << "    if (fwrite(text, 1, n, stdout) != n) return 1;\n"
        << "    return putchar('\\n') == EOF;\n"
        << "}\n"
        << "static void qs_rt_report(int status) {\n"
        << "    (void)status;\n"
        << "    if (qs_rt_message[0]) fprintf(stderr, \"qscript: %s\\n\", qs_rt_message);\n"
        << "}\n"
        << "#endif\n";
    return out.str();
}

// Tables and constants that describe the unit to the C run-time.
Result<std::string> c_prelude(const Unit& unit, const Options& o) {
    auto table = detail::run_time_types(unit);
    if (!table) return {std::nullopt, std::move(table.diagnostics)};
    std::ostringstream out;
    out << "#include <ctype.h>\n#include <stdarg.h>\n"
        << "#define QS_BUILTIN_BASE ((int64_t)1 << 40)\n";
    for (const auto& c : detail::runtime_builtins)
        out << "#define QS_B_" << c.name << ' ' << static_cast<std::int64_t>(c.id) << "\n";
    out << "#define QS_K_INT " << static_cast<int>(Type::Kind::Int) << "\n"
        << "#define QS_K_STR " << static_cast<int>(Type::Kind::Str) << "\n"
        << "#define QS_K_LIST " << static_cast<int>(Type::Kind::List) << "\n"
        << "#define QS_NSTRINGS " << unit.strings.size() << "\n"
        << "#define QS_NTYPES " << table->size() << "\n"
        << "#define QS_MAX_OBJECTS " << o.max_heap_objects << "ULL\n"
        << "#define QS_MAX_BYTES " << o.max_heap_bytes << "ULL\n"
        << "typedef int (*qs_generated_text_fn)(const char *, size_t, void *);\n"
        << "struct qs_string { const char *p; size_t n; };\n"
        << "struct qs_type { int kind; int elem; };\n";
    out << "static const struct qs_string qs_strings[] = {\n";
    for (const auto& s : unit.strings) {
        out << '{';
        c_string(out, s);
        out << ',' << s.size() << "},\n";
    }
    out << "{\"\",0}};\n";
    out << "static const struct qs_type qs_types[] = {\n";
    for (const auto& e : *table) out << '{' << e.kind << ',' << e.elem << "},\n";
    out << "{0,-1}};\n";
    out << c_runtime_source_text();
    return {out.str(), {}};
}

std::string c_bridge_prelude(const Unit& unit, const Options& o) {
    std::ostringstream out;
    out << "/* QScript linked runtime: link MetaChain::QScript (C++ grammar engine). */\n"
        << "#include \"qscript/include/QScript-Generated.h\"\n"
        << "typedef int (*qs_generated_text_fn)(const char *, size_t, void *);\n"
        << "static const qs_runtime_string qs_bridge_strings[] = {\n";
    for (const auto& s : unit.strings) { out << '{'; c_string(out, s); out << ',' << s.size() << "},\n"; }
    out << "{\"\",0}};\nstatic const char *const qs_bridge_types[] = {\n";
    for (const auto& type : unit.types) { c_string(out, type); out << ",\n"; }
    out << "\"int\"};\nstatic const qs_runtime_grammar qs_bridge_grammars[] = {\n";
    for (const auto& g : unit.grammars) {
        out << '{'; c_string(out, g.name); out << ','; c_string(out, g.source); out << ',' << g.source.size() << "},\n";
    }
    out << "{\"\",\"\",0}};\nstatic const qs_runtime_config qs_bridge_config = {\n"
        << "qs_bridge_strings," << unit.strings.size() << ",qs_bridge_types," << unit.types.size()
        << ",qs_bridge_grammars," << unit.grammars.size() << ',' << o.max_heap_objects << "ULL," << o.max_heap_bytes << "ULL};\n";
    out << R"BRIDGE(
static qs_context *qs_bridge_context = NULL;
static qs_runtime *qs_bridge_runtime = NULL;
static qs_generated_text_fn qs_rt_text = NULL;
static void *qs_rt_text_user = NULL;
static qs_generated_output_fn qs_bridge_output = NULL;
static void *qs_bridge_user = NULL;
static int qs_bridge_callback_failed = 0;
static char qs_rt_message[512] = "";
void qs_generated_set_text(qs_generated_text_fn text, void *user) {
    qs_rt_text = text; qs_rt_text_user = user;
}
static void qs_bridge_text(const char *text, size_t length, void *user) {
    (void)user;
    if (qs_rt_text && qs_rt_text(text, length, qs_rt_text_user)) qs_bridge_callback_failed = 1;
}
static void qs_bridge_integer(int64_t value, void *user) {
    (void)user;
    if (qs_bridge_output && qs_bridge_output(value, qs_bridge_user)) qs_bridge_callback_failed = 1;
}
static void qs_rt_end(void) {
    qs_runtime_destroy(qs_bridge_runtime); qs_bridge_runtime = NULL;
    qs_destroy(qs_bridge_context); qs_bridge_context = NULL;
}
static int qs_rt_begin(qs_generated_output_fn output, void *user) {
    qs_rt_end();
    qs_rt_message[0] = '\0'; qs_bridge_callback_failed = 0;
    qs_bridge_output = output; qs_bridge_user = user;
    qs_bridge_context = qs_create();
    if (!qs_bridge_context) { snprintf(qs_rt_message, sizeof qs_rt_message, "out of memory"); return 3; }
    qs_set_output(qs_bridge_context, qs_bridge_integer, NULL);
    if (qs_rt_text) qs_set_text_output(qs_bridge_context, qs_bridge_text, NULL);
    qs_bridge_runtime = qs_runtime_create(qs_bridge_context, &qs_bridge_config);
    if (!qs_bridge_runtime) {
        snprintf(qs_rt_message, sizeof qs_rt_message, "%s", qs_error(qs_bridge_context));
        return 3;
    }
    return 0;
}
static int qs_rt_builtin(int64_t id, const int64_t *args, int64_t *value, void *user) {
    (void)user;
    if (id < ((int64_t)1 << 40) || id >= ((int64_t)1 << 40) + 256) return 0;
    if (qs_runtime_dispatch(qs_bridge_context, qs_bridge_runtime, id, args, 32, value) != QS_OK) {
        snprintf(qs_rt_message, sizeof qs_rt_message, "%s", qs_error(qs_bridge_context));
        return strstr(qs_rt_message, "heap ") ? -3 : -1;
    }
    if (qs_bridge_callback_failed) {
        snprintf(qs_rt_message, sizeof qs_rt_message, "output callback failed"); return -4;
    }
    return 1;
}
#ifdef QSCRIPT_STANDALONE
static int qs_rt_stdout_text(const char *text, size_t length, void *user) {
    (void)user;
    if (fwrite(text, 1, length, stdout) != length) return 1;
    return putchar('\n') == EOF;
}
static void qs_rt_report(int status) {
    (void)status;
    if (qs_rt_message[0]) fprintf(stderr, "qscript: %s\n", qs_rt_message);
}
#endif
)BRIDGE";
    return out.str();
}
} // namespace

Result<std::string> emit_c(const Unit& unit, Options o) {
    const auto& p = unit.program;
    const bool builtins = detail::uses_builtins(p);
    auto full = detail::needs_full_runtime(unit);
    if (!full) return {std::nullopt, std::move(full.diagnostics)};
    const bool bridge = *full;
    auto verified = verify_ir(p, o);
    if (!verified) return {std::nullopt, std::move(verified.diagnostics)};

    metachain::CRuntime runtime;
    if (builtins) {
        if (bridge) runtime.prelude = c_bridge_prelude(unit, o);
        else {
            auto prelude = c_prelude(unit, o);
            if (!prelude) return {std::nullopt, std::move(prelude.diagnostics)};
            runtime.prelude = std::move(*prelude);
        }
        runtime.begin = "status=qs_rt_begin(output,user); if (status) goto done;";
        runtime.end = "qs_rt_end();";
        runtime.builtin = "qs_rt_builtin";
        runtime.standalone = "qs_generated_set_text(qs_rt_stdout_text,NULL);";
        runtime.report = "qs_rt_report";
    }
    auto source = metachain::lower_c99(p, o.toolchain.vm, o.toolchain.assembler.max_source_bytes, runtime);
    Result<std::string> result;
    for (const auto& d : source.diagnostics)
        result.diagnostics.push_back({metachain::Stage::Lower, d.message, d.source.file, d.source.line, d.source.column, d.instruction});
    if (source) result.value = std::move(source.source);
    return result;
}
} // namespace qscript
