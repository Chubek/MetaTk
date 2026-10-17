#ifndef METACHAIN_QSCRIPT_FFI_INTERNAL_H
#define METACHAIN_QSCRIPT_FFI_INTERNAL_H
#include "qscript/src/Context.h"
#include "qscript/include/QScript-CFFI.h"
#include <memory>
namespace qscript::detail {
struct SharedLibrary {
    void *handle = nullptr;
    ~SharedLibrary();
};
std::shared_ptr<SharedLibrary> open_library(const char *path);
void *library_symbol(const SharedLibrary&, const char *name);
void register_host(qs_context *, std::string name, std::size_t arity,
                   std::function<std::int64_t(std::span<const std::int64_t>)>);
class Session;
// Install the host bindings (and, when `session` is given, the built-in
// dispatcher) into the VM options.
Options runtime_options(Options, Session *session = nullptr);
}
struct qs_shared_library { std::shared_ptr<qscript::detail::SharedLibrary> library; };
#endif
