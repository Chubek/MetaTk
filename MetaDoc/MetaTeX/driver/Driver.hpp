#pragma once
#include "MetaTeX-Device.h"
#include <dynalo/dynalo.hpp>
namespace metatex::driver {
// Declare before Engine so loaded code outlives registered handler destruction.
struct PluginLibraries {
    std::vector<std::unique_ptr<dynalo::library>> handles;
    std::vector<std::string> names;
    ~PluginLibraries();
};
void loadPlugin(const std::string& name, const std::filesystem::path& executable, PluginLibraries& libraries, Engine& engine);
void loadLaTeX(const std::filesystem::path& executable, PluginLibraries& libraries, Engine& engine);
void loadNekoTeX(const std::filesystem::path& executable, PluginLibraries& libraries, Engine& engine);
std::filesystem::path defaultOutput(const std::filesystem::path& input, std::string_view target);
void checkOutputPath(const std::filesystem::path& input, const std::filesystem::path& output);
bool incompleteSource(const std::string& source);
}
