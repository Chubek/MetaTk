#ifndef METAPP_HPP
#define METAPP_HPP
#include "MetaPP.h"
#include <stdexcept>
#include <string>
#include <utility>
#include <new>
namespace metapp {
class Preprocessor {
    metapp_context *ctx_ = metapp_create();
    void check(int ok) const { if (!ok) throw std::runtime_error(metapp_diagnostics(ctx_)); }
    static const char *c_text(const std::string &text) {
        if (text.find('\0') != std::string::npos) throw std::runtime_error("embedded NUL in MetaPP argument");
        return text.c_str();
    }
public:
    Preprocessor() { if (!ctx_) throw std::bad_alloc(); }
    ~Preprocessor() { metapp_destroy(ctx_); }
    Preprocessor(const Preprocessor &) = delete;
    Preprocessor &operator=(const Preprocessor &) = delete;
    Preprocessor(Preprocessor &&other) noexcept : ctx_(other.ctx_) { other.ctx_ = nullptr; }
    Preprocessor &operator=(Preprocessor &&other) noexcept { std::swap(ctx_, other.ctx_); return *this; }
    void define(const std::string &text) { check(metapp_define(ctx_, c_text(text))); }
    void undefine(const std::string &name) { check(metapp_undefine(ctx_, c_text(name))); }
    void include_path(const std::string &path) { check(metapp_add_include_path(ctx_, c_text(path))); }
    std::string preprocess(const std::string &source, const std::string &file = "<stdin>") {
        char *out = nullptr;
        check(metapp_preprocess_n(ctx_, source.data(), source.size(), c_text(file), &out));
        try { std::string result(out); metapp_free(out); return result; }
        catch (...) { metapp_free(out); throw; }
    }
    std::string diagnostics() const { return metapp_diagnostics(ctx_); }
};
}
#endif
