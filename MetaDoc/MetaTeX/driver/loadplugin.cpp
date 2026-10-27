#include "Driver.hpp"
#include <algorithm>
#include <cstdlib>
#include <stdexcept>
namespace metatex::driver {
PluginLibraries::~PluginLibraries() { while (!handles.empty()) handles.pop_back(); }
void loadPlugin(const std::string& name, const std::filesystem::path& exe, PluginLibraries& libraries, Engine& engine) {
    if (name.empty() || !std::all_of(name.begin(), name.end(), [](unsigned char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
    })) throw std::runtime_error("invalid plugin name: " + name);
    if (std::find(libraries.names.begin(), libraries.names.end(), name) != libraries.names.end()) return;
    const std::string file = "metatex_" + name + METATEX_PLUGIN_SUFFIX;
    std::vector<std::filesystem::path> paths{exe.parent_path() / file,
        exe.parent_path().parent_path() / "lib" / "metatex" / file, std::filesystem::current_path() / file};
    if (const char* env = std::getenv("METATEX_PLUGIN_PATH")) {
        std::string list = env; std::size_t at = 0;
        while (at < list.size()) {
            auto end = list.find(':', at); if (end == std::string::npos) end = list.size();
            if (end > at) paths.push_back(std::filesystem::path(list.substr(at, end - at)) / file);
            at = end + 1;
        }
    }
    std::string errors;
    for (const auto& path : paths) {
        std::unique_ptr<dynalo::library> handle;
        try { handle = std::make_unique<dynalo::library>(path.string()); }
        catch (const std::exception& error) { errors += path.string() + ": " + error.what() + '\n'; continue; }
        auto id = handle->get_function<const char*()>("metatex_plugin_id");
        auto initialize = handle->get_function<void(Engine*)>("metatex_register_plugin");
        const char* pluginId = id ? id() : nullptr;
        if (!pluginId || !initialize || name != pluginId) throw std::runtime_error("incompatible plugin: " + path.string());
        // Retain library even if initialization throws after registering handlers.
        libraries.handles.push_back(std::move(handle));
        initialize(&engine); libraries.names.push_back(name); return;
    }
    throw std::runtime_error("unable to load " + name + " plugin; build/install it with MetaTeX:\n" + errors);
}
}
