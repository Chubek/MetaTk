#ifndef METAJIT_METHOD_BASED_JIT_HPP
#define METAJIT_METHOD_BASED_JIT_HPP

#include "Runtime.hpp"
#include <functional>
#include <map>
#include <optional>
#include <stdexcept>
#include <utility>

namespace metajit {
/** Lazy, whole-method compilation cache over the trusted CodeBuffer interface.
 * A compiler must emit an entry matching the registered name. Compilation runs
 * on the first get(); failures are propagated and may be retried. Successful
 * code is owned until cache destruction, so borrowed pointers remain stable.
 * Registration/get/compiled require external synchronization. Calls through
 * published native pointers may run concurrently while the owner stays alive.
 * This is not an IR compiler, tracing engine, or an executable-code verifier. */
class MethodBasedJIT {
public:
    using Compiler = std::function<CodeBuffer()>;
    MethodBasedJIT() = default;
    MethodBasedJIT(const MethodBasedJIT &) = delete;
    MethodBasedJIT &operator=(const MethodBasedJIT &) = delete;

    void register_method(std::string name, Compiler compiler) {
        if (name.empty() || name.find('\0') != name.npos || !compiler)
            throw std::invalid_argument("MetaJIT: invalid method registration");
        if (methods_.count(name)) throw std::invalid_argument("MetaJIT: duplicate method: " + name);
        methods_.emplace(std::move(name), Method{std::move(compiler), {}, false});
    }

    template<class Signature> Signature get(std::string_view name) {
        auto it = methods_.find(name);
        if (it == methods_.end()) throw std::invalid_argument("MetaJIT: unknown method: " + std::string(name));
        auto &method = it->second;
        if (method.code) return method.code->get<Signature>(name);
        if (method.compiling) throw std::logic_error("MetaJIT: recursive method compilation: " + std::string(name));
        method.compiling = true;
        struct Reset { bool &flag; ~Reset() { flag = false; } } reset{method.compiling};
        auto code = method.compiler().finalize();
        // Validate before publishing: missing entries/wrong signatures must not
        // poison the cache, and a failed compilation must release its mapping.
        auto function = code.get<Signature>(name);
        method.code.emplace(std::move(code));
        return function;
    }

    bool compiled(std::string_view name) const {
        auto it = methods_.find(name);
        return it != methods_.end() && it->second.code.has_value();
    }

private:
    struct Method {
        Compiler compiler;
        std::optional<Executable> code;
        bool compiling;
    };
    std::map<std::string, Method, std::less<>> methods_;
};
} // namespace metajit
#endif
