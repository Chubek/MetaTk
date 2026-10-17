#include "qscript/include/QScript-Native.h"
#include "qscript/src/Runtime.h"
#include "qscript/src/RuntimeConfig.h"
#include <cerrno>
#include <cstring>
#include <new>
#if defined(__linux__)
#include <unistd.h>
#endif

namespace {
bool write_bytes(int fd, std::string_view text) noexcept {
#if defined(__linux__)
    while (!text.empty()) {
        auto n = ::write(fd, text.data(), text.size());
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return false;
        text.remove_prefix(static_cast<std::size_t>(n));
    }
    return true;
#else
    (void)fd; (void)text; return false;
#endif
}
void report(std::string_view message) noexcept {
    (void)write_bytes(2, "qscript: "); (void)write_bytes(2, message); (void)write_bytes(2, "\n");
}
struct OutputFailure : std::exception { const char* what() const noexcept override { return "output write failed"; } };
struct State {
    qscript::Unit unit;
    std::unique_ptr<qscript::detail::Session> session;
};
int failure(const std::exception& error, int status = 126) noexcept {
    report(error.what());
    return status == 126 && std::strstr(error.what(), "heap ") ? 125 : status;
}
}
extern "C" int qs_native_launch(const qs_native_unit* config, qs_native_entry_fn entry) {
    static_assert(sizeof(void*) == 8 && sizeof(qs_runtime_config) == 64 && sizeof(qs_native_unit) == 88,
                  "the native runtime ABI is Linux x86-64");
    static_assert(offsetof(qs_native_unit, runtime) == 8 && offsetof(qs_native_unit, max_source_bytes) == 72 &&
                  offsetof(qs_native_unit, max_table_entries) == 80);
    if (!config || config->abi != QS_NATIVE_ABI || !entry) { report("invalid native runtime ABI"); return 126; }
    try {
        qscript::Options options;
        options.toolchain.il.max_source_bytes = config->max_source_bytes;
        options.toolchain.il.max_instructions = config->max_table_entries;
        options.max_heap_objects = config->runtime.max_heap_objects;
        options.max_heap_bytes = config->runtime.max_heap_bytes;
        options.text_output = [](std::string_view text) {
            if (!write_bytes(1, text) || !write_bytes(1, "\n")) throw OutputFailure{};
        };
        auto unit = qscript::detail::runtime_unit(config->runtime, options);
        if (!unit) { report(unit.diagnostics.front().message); return 126; }
        State state{std::move(*unit), {}};
        state.session = std::make_unique<qscript::detail::Session>(state.unit, options);
        // All arithmetic/control flow is in the emitted native entry. This
        // launcher only owns its built-in heap and maps its exit value.
        return static_cast<unsigned char>(entry(&state));
    } catch (const OutputFailure& error) { return failure(error, 127); }
    catch (const std::bad_alloc& error) { return failure(error, 125); }
    catch (const std::exception& error) { return failure(error); }
    catch (...) { report("unexpected native runtime failure"); return 126; }
}
extern "C" int qs_native_dispatch(void* runtime, int64_t id, const int64_t* registers, int64_t* result) {
    if (result) *result = 0;
    if (!runtime || !registers || !result || !qscript::detail::is_builtin_id(id)) {
        report("invalid native runtime dispatch"); return 126;
    }
    try {
        auto& state = *static_cast<State*>(runtime);
        *result = state.session->dispatch(id, std::span(registers, 32));
        return 0;
    } catch (const OutputFailure& error) { return failure(error, 127); }
    catch (const std::bad_alloc& error) { return failure(error, 125); }
    catch (const std::exception& error) { return failure(error); }
    catch (...) { report("unexpected native built-in failure"); return 126; }
}
