#include "Driver.hpp"
namespace metatex::driver {
void loadLaTeX(const std::filesystem::path& executable, PluginLibraries& libraries, Engine& engine) {
    loadPlugin("latex", executable, libraries, engine);
}
}
