#pragma once
#include "MetaTeX.h"
#include <chrono>

namespace metatex::detail {
struct ScriptOptions {
    int descriptor=1;
    std::chrono::milliseconds timeout{30000};
    std::size_t outputLimit=4*1024*1024;
};
std::string runExternal(const std::filesystem::path& script,const std::vector<std::string>& arguments,const ScriptOptions& options);
std::string runMetaML(std::string_view source,const std::vector<std::string>& arguments,const std::filesystem::path& filename);
}
