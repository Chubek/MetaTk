#include "Driver.hpp"
namespace metatex::driver {
void loadNekoTeX(const std::filesystem::path& executable, PluginLibraries& libraries, Engine& engine) {
    loadLaTeX(executable, libraries, engine); loadPlugin("nekotex", executable, libraries, engine);
}
}
