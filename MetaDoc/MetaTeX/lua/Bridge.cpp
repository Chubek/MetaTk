#include "lmtex.h"
#include "MetaTeX-Device.h"
#include "../utils/Diagnostics.hpp"
#include <cstdlib>
#include <cstring>
#include <new>
struct lmtex_engine { metatex::Engine engine; };
namespace {
char* copy(std::string_view text) {
    auto* result = static_cast<char*>(std::malloc(text.size() + 1));
    if (!result) throw std::bad_alloc();
    std::memcpy(result, text.data(), text.size()); result[text.size()] = 0; return result;
}
}
extern "C" {
lmtex_engine* lmtex_create(void) {
    try { return new lmtex_engine; } catch (...) { return nullptr; }
}
void lmtex_destroy(lmtex_engine* engine) { delete engine; }
void lmtex_release(lmtex_result* result) {
    if (!result) return;
    std::free(result->output); std::free(result->diagnostics); *result = {};
}
lmtex_result lmtex_compile(lmtex_engine* engine, const char* source, size_t length,
    const char* filename, const char* device, int fragment) {
    lmtex_result result{};
    try {
        if (!engine || (!source && length)) throw std::invalid_argument("invalid Lua engine or source");
        if (length > 16u * 1024u * 1024u) throw std::length_error("Lua source limit exceeded");
        std::string_view target = device ? device : "html";
        (void)metatex::findDevice(target);
        std::string input(source ? source : "", length);
        const auto file = filename ? filename : "<lua>";
        auto doc = fragment ? engine->engine.compileFragment(std::move(input), file) : engine->engine.compile(std::move(input), file);
        std::string diagnostics;
        for (const auto& d : doc.diagnostics) diagnostics += metatex::util::formatDiagnostic(d) + '\n';
        result.diagnostics = copy(diagnostics);
        if (metatex::util::hasErrors(doc)) result.status = 1;
        else { auto bytes = metatex::renderDevice(doc, target); result.output = copy(bytes); result.output_size = bytes.size(); }
    } catch (const std::exception& error) {
        lmtex_release(&result); result.status = 2;
        try { result.diagnostics = copy(error.what()); } catch (...) {}
    } catch (...) { lmtex_release(&result); result.status = 2; }
    return result;
}
}
