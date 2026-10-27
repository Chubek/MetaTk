#include "Scripting.h"
#include "../command/Extern.hpp"
namespace metatex::detail {
std::string runExternal(const std::filesystem::path& script, const std::vector<std::string>& arguments, const ScriptOptions& options) {
    return command::runExternal(script, arguments, options);
}
}
