#ifndef METAIL_HPP
#define METAIL_HPP
#include "MetaIL.h"
#include <fstream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace metail {
class Error : public std::runtime_error {
public:
    explicit Error(const std::string &message) : std::runtime_error(message) {}
};

struct OptimizationResult {
    std::string source;
    std::string stats; // JSON: {"kernel_changes": {name: count, ...}}
};

// Move-only RAII owner. All functions throw Error on runtime/argument failures.
// A context may be reused after an error; serialize calls to a single context.
class Context {
    std::unique_ptr<metail_context, decltype(&metail_destroy)> context_{nullptr,metail_destroy};
    using String = std::unique_ptr<char, decltype(&metail_free)>;
    static const char *cstring(const std::string &s) {
        if (s.find('\0')!=std::string::npos) throw Error("embedded NUL in MetaIL argument");
        return s.c_str();
    }
    metail_context *get() const {
        if (!context_) throw Error("moved-from MetaIL context");
        return context_.get();
    }
    void check_status(metail_status status) const {
        if (status!=METAIL_OK) throw Error(metail_last_error(get()));
    }
public:
    // Empty home selects automatic runtime discovery; explicit paths are strict.
    explicit Context(const std::string &home={}) {
        char error[2048];
        context_.reset(metail_create(home.empty() ? nullptr : cstring(home),error,sizeof error));
        if (!context_) throw Error(error);
    }
    Context(Context &&) noexcept = default;
    Context &operator=(Context &&) noexcept = default;
    Context(const Context &) = delete;
    Context &operator=(const Context &) = delete;
    metail_context *native_handle() const { return get(); } // Borrowed; do not destroy.

    void check(const std::string &source, const std::string &name="<memory>") const {
        check_status(metail_check(get(),cstring(source),cstring(name)));
    }
    std::string emit(const std::string &source, const std::string &name="<memory>") const {
        char *output=nullptr;
        auto status=metail_emit(get(),cstring(source),cstring(name),&output);
        String owned(output,metail_free);check_status(status);return owned.get();
    }
    OptimizationResult optimize(const std::string &source, const std::string &passes="all",
                                int rounds=2, const std::string &name="<memory>") const {
        char *output=nullptr,*stats=nullptr;
        auto status=metail_optimize(get(),cstring(source),cstring(name),cstring(passes),rounds,&output,&stats);
        String text(output,metail_free),counts(stats,metail_free);check_status(status);
        return {text.get(),counts.get()};
    }
    std::string analyze(const std::string &source, const std::string &passes="all",
                        const std::string &name="<memory>") const {
        char *output=nullptr;
        auto status=metail_analyze(get(),cstring(source),cstring(name),cstring(passes),&output);
        String owned(output,metail_free);check_status(status);return owned.get();
    }
    void load_kernels(const std::string &directory) const {
        check_status(metail_load_kernels(get(),cstring(directory)));
    }
    std::string kernels_json() const {
        char *output=nullptr;
        auto status=metail_kernels_json(get(),&output);
        String owned(output,metail_free);check_status(status);return owned.get();
    }
    static std::string read_file(const std::string &path) {
        std::ifstream file(cstring(path),std::ios::binary);
        if (!file) throw Error("cannot open "+path);
        std::ostringstream contents;contents<<file.rdbuf();
        if (file.bad() || contents.bad()) throw Error("cannot read "+path);
        return contents.str();
    }
};
} // namespace metail
#endif
