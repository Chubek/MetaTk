#pragma once
#include "MetaTeX.h"
#include <chrono>
#include "../command/Shell.hpp"

namespace metatex::detail {
using ScriptOptions = command::ProcessOptions;
std::string runExternal(const std::filesystem::path& script,const std::vector<std::string>& arguments,const ScriptOptions& options);
std::string runMetaML(std::string_view source,const std::vector<std::string>& arguments,const std::filesystem::path& filename);
}
