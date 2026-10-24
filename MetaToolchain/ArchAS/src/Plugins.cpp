#include "Internal.hpp"
#include "../ArchASPlugin.h"
#if ARCHAS_PLUGINS
#include <dlfcn.h>
#include <cstring>
#endif

namespace archas {
bool has_plugins() noexcept { return ARCHAS_PLUGINS != 0; }
namespace detail {
#if ARCHAS_PLUGINS
struct Plugins::Impl {
    struct Module {
        void *library = nullptr, *state = nullptr;
        const ArchASPlugin *plugin = nullptr;
        ArchASHost host{};
        bool initialized = false;
        ~Module() {
            if (plugin && plugin->shutdown && (initialized || state)) plugin->shutdown(state);
            if (library) dlclose(library);
        }
    };
    struct Emission { std::string text, error; size_t limit; };
    const mair::Target &target;
    const Options &options;
    Directives &directives;
    std::string error;
    bool registering = false;
    std::vector<std::unique_ptr<Module>> modules;

    static void message(void *context, const char *text) noexcept {
        try { static_cast<Impl *>(context)->error = text ? text : "plugin failed"; } catch (...) {}
    }
    static int emit(void *context, const char *text, size_t length) noexcept {
        auto &out = *static_cast<Emission *>(context);
        try {
            if (!text && length) throw Error("plugin emitted a NULL buffer");
            append(out.text, {text ? text : "", length}, out.limit); return 0;
        } catch (const std::exception &e) { try { out.error = e.what(); } catch (...) {} }
        return -1;
    }
    static void fail(void *context, const char *text) noexcept {
        try { static_cast<Emission *>(context)->error = text ? text : "plugin directive failed"; } catch (...) {}
    }
    static int register_directive(void *context, const char *name, ArchASDirective fn, void *user) noexcept {
        auto &s = *static_cast<Impl *>(context);
        try {
            if (!s.registering) throw Error("plugin registration is only allowed during init");
            if (!name || !fn) throw Error("invalid plugin directive registration");
            auto key = directive_name(name);
            if (s.directives.contains(key)) throw Error("duplicate directive: " + key);
            size_t limit = s.options.limits.bytes;
            s.directives.emplace(key, [fn, user, limit](std::string_view args, const DirectiveContext &ctx) {
                Emission output{{}, {}, limit};
                const std::string arguments(args); // NUL-terminated in addition to the ABI length.
                ArchASInvocation invocation{arguments.c_str(), arguments.size(), ctx.location.file.c_str(),
                    ctx.location.line, ctx.location.column, ctx.target.name().c_str(), ctx.syntax == Syntax::intel ? "intel" : "att"};
                const ArchASEmitter emitter{&output, emit, fail};
                int rc = fn(user, &invocation, &emitter);
                if (rc || !output.error.empty()) throw Error(output.error.empty() ? "plugin directive failed" : output.error, ctx.location);
                return std::move(output.text);
            });
            return 0;
        } catch (const std::exception &e) { message(context, e.what()); }
        return -1;
    }
    Impl(const mair::Target &t, const Options &o, Directives &d) : target(t), options(o), directives(d) {
        for (const auto &path : options.plugins) {
            auto module = std::make_unique<Module>();
            auto absolute = std::filesystem::absolute(path).string();
            module->library = dlopen(absolute.c_str(), RTLD_NOW | RTLD_LOCAL);
            if (!module->library) throw IOError("cannot load plugin " + absolute + ": " + dlerror());
            dlerror();
            void *symbol = dlsym(module->library, "archas_plugin_v1");
            if (const char *why = dlerror()) throw Error("plugin " + absolute + ": " + why);
            ArchASPluginEntry entry = nullptr;
            static_assert(sizeof entry == sizeof symbol);
            std::memcpy(&entry, &symbol, sizeof entry);
            const auto *plugin = entry();
            if (!plugin || plugin->abi_version != ARCHAS_PLUGIN_ABI || plugin->struct_size < sizeof(ArchASPlugin) ||
                !plugin->name || !plugin->init) throw Error("plugin ABI mismatch: " + absolute);
            module->plugin = plugin;
            module->host = {ARCHAS_PLUGIN_ABI, sizeof(ArchASHost), this, register_directive, message,
                            target.name().c_str(), options.syntax == Syntax::intel ? "intel" : "att"};
            registering = true;
            int rc = plugin->init(&module->host, &module->state);
            registering = false;
            if (rc || !error.empty()) throw Error("plugin " + absolute + ": " + (error.empty() ? "initialization failed" : error));
            module->initialized = true;
            modules.push_back(std::move(module));
        }
    }
};
Plugins::Plugins(const mair::Target &t, const Options &o, Directives &d) : impl_(std::make_unique<Impl>(t, o, d)) {}
#else
struct Plugins::Impl {};
Plugins::Plugins(const mair::Target &, const Options &o, Directives &) {
    if (!o.plugins.empty()) throw Error("shared-library plugins were disabled at build time");
}
#endif
Plugins::~Plugins() = default;
} // namespace detail
} // namespace archas
