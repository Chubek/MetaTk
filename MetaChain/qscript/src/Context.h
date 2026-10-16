#ifndef METACHAIN_QSCRIPT_CONTEXT_H
#define METACHAIN_QSCRIPT_CONTEXT_H
// Definitions of the opaque C handles (not installed API).
#include "qscript/QScript.h"
#include <string>
#include <vector>

struct qs_context {
    qscript::Options options;
    std::string error;
    std::vector<std::uint8_t> buffer;
    std::vector<qscript::Diagnostic> diagnostics;
    qs_output_fn output = nullptr;
    void* user = nullptr;
    int engine = 0;  // qs_engine
    // Library resolver callback (QScript-Library.h).
    const char* (*resolver)(const char*, size_t*, void*) = nullptr;
    void* resolver_user = nullptr;
};
struct qs_program { qscript::Unit unit; };

namespace qscript::detail {
// Run fn with the context's error state reset; exceptions become QS_ERROR.
template<class Function> qs_status protect(qs_context* c, Function fn) noexcept {
    if (!c) return QS_INVALID_ARGUMENT;
    c->error.clear(); c->buffer.clear(); c->diagnostics.clear();
    try { return fn(); }
    catch (const std::exception& e) { try { c->error = e.what(); } catch (...) {} }
    catch (...) { try { c->error = "unexpected QScript failure"; } catch (...) {} }
    return QS_ERROR;
}
inline void report(qs_context* c, const std::vector<Diagnostic>& diagnostics) {
    for (const auto& d : diagnostics) {
        if (!c->error.empty()) c->error += '\n';
        c->error += metachain::format(d);
        c->diagnostics.push_back(d);
    }
}
inline qs_status text_out(qs_context* c, const std::string& text, const char** data, size_t* length) {
    c->buffer.assign(text.begin(), text.end());
    c->buffer.push_back(0);
    *data = reinterpret_cast<const char*>(c->buffer.data());
    *length = text.size();
    return QS_OK;
}
// Options with the context's resolver callback bound in.
Options effective_options(const qs_context* c);
} // namespace qscript::detail
#endif
