#include "MetaML.h"
#include "../cpp/Metalib.h"
#include <cstdlib>
#include <cstring>
#include <new>

struct metaml_c_session {
    metaml::Session runtime;
    std::string error;
    std::string output;
    metaml_c_session() {
        runtime.set_output([this](std::string_view s) { output.append(s); });
    }
};

namespace {
void message(metaml_c_session *s, const char *text) noexcept {
    if (s) try { s->error = text; } catch (...) {}
}
template<class F> metaml_status boundary(metaml_c_session *s, F f) noexcept {
    if (!s) return METAML_INVALID_ARGUMENT;
    try {
        s->error.clear();
        return f();
    } catch (const std::exception &e) {
        message(s, e.what());
    } catch (...) {
        message(s, "unknown host failure");
    }
    return METAML_HOST_ERROR;
}
metaml_status invalid(metaml_c_session *s) noexcept {
    message(s, "invalid argument");
    return METAML_INVALID_ARGUMENT;
}
metaml_status errors(metaml_c_session *s, const std::vector<metaml::Diagnostic> &ds) {
    for (const auto &d : ds) s->error += s->runtime.render(d);
    return METAML_LANGUAGE_ERROR;
}
void copy_text(std::string_view text, metaml_text *out) {
    auto p = static_cast<char *>(std::malloc(text.size() + 1));
    if (!p) throw std::bad_alloc();
    if (!text.empty()) std::memcpy(p, text.data(), text.size());
    p[text.size()] = '\0';
    out->data = p;
    out->size = text.size();
}
}

extern "C" {
void metaml_text_free(metaml_text *text) {
    if (text) { std::free(text->data); *text = {}; }
}
metaml_status metaml_session_create(metaml_c_session **out) {
    if (!out) return METAML_INVALID_ARGUMENT;
    *out = nullptr;
    try { *out = new metaml_c_session; return METAML_OK; }
    catch (...) { return METAML_HOST_ERROR; }
}
void metaml_session_destroy(metaml_c_session *s) { delete s; }
const char *metaml_c_version(void) { return metaml::version; }
const char *metaml_last_error(const metaml_c_session *s) {
    return s ? s->error.c_str() : "invalid session";
}
metaml_status metaml_c_load_stdlib(metaml_c_session *s, const char *dir) {
    return boundary(s, [&] {
        auto r = s->runtime.load_stdlib(dir ? dir : "");
        return r.is_ok() ? METAML_OK : errors(s, r.unwrap_err());
    });
}
metaml_status metaml_install_metalib(metaml_c_session *s) {
    return boundary(s, [&] { metalib::install_all(s->runtime); return METAML_OK; });
}
metaml_status metaml_allow_primitives(metaml_c_session *s, const char *const *names, size_t n) {
    return boundary(s, [&] {
        if (n && !names) return invalid(s);
        std::vector<std::string> allow;
        for (size_t i = 0; i < n; ++i) {
            if (!names[i]) return invalid(s);
            allow.emplace_back(names[i]);
        }
        s->runtime.set_primitive_allowlist(std::move(allow));
        return METAML_OK;
    });
}
metaml_status metaml_c_eval(metaml_c_session *s, const char *src, size_t n, metaml_text *out) {
    if (out) *out = {};
    return boundary(s, [&] {
        if (!out || (!src && n)) return invalid(s);
        auto r = s->runtime.eval(std::string_view(src ? src : "", n));
        if (r.is_err()) return errors(s, r.unwrap_err());
        copy_text(metaml::show_value(r.unwrap()), out);
        return METAML_OK;
    });
}
metaml_status metaml_c_type_of(metaml_c_session *s, const char *src, size_t n, metaml_text *out) {
    if (out) *out = {};
    return boundary(s, [&] {
        if (!out || (!src && n)) return invalid(s);
        auto r = s->runtime.type_of(std::string_view(src ? src : "", n));
        if (r.is_err()) return errors(s, r.unwrap_err());
        copy_text(r.unwrap(), out);
        return METAML_OK;
    });
}
metaml_status metaml_output(metaml_c_session *s, metaml_text *out, int clear) {
    if (out) *out = {};
    return boundary(s, [&] {
        if (!out) return invalid(s);
        copy_text(s->output, out);
        if (clear) s->output.clear();
        return METAML_OK;
    });
}
}
